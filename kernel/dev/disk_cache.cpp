#include "disk_cache.hpp"
#include "ide.hpp"
#include "timer.hpp"
#include "lang.hpp"
#include "memory/include/vm.hpp"
#include "process/include/thread.hpp"
#include "lock_guard.hpp"

using namespace UOS;

inline qword page_lba(qword lba){
	return align_down(lba,PAGE_SIZE/SECTOR_SIZE);
}

disk_cache::slot::slot(qword va) : access((void*)va),phy_page(pm.allocate(PM::MUST_SUCCEED)){
	//TODO allocate phy_page within 4G range
	auto res = vm.assign(va,phy_page,1);
	if (!res)
		bugcheck("disk_cache::slot failed %x,%x",va,(qword)phy_page);
}

bool disk_cache::slot::match(qword aligned_lba) const{
	assert(aligned_lba == page_lba(aligned_lba));
	if (valid == 0)
		return false;
	return lba_base == aligned_lba;
}

void disk_cache::slot::flush(void){
	assert(objlock.is_locked() && objlock.is_exclusive());
	if (dirty == 0){
		valid = 0;
		return;
	}
	unsigned head = 0;
	for (unsigned i = 0;i <= 8;++i){
		byte mask = 1 << i;
		if (0 == (dirty & mask)){
			if (head != i){
				// [head , i - 1]
				auto res = ide.write(lba_base + head,phy_page + SECTOR_SIZE*head,SECTOR_SIZE*(i - head));
				if (!res){
					//TODO handle failure
					bugcheck("disk write failed @ LBA %x",lba_base + head);
				}
			}
			head = i + 1;
		}
	}
	dirty = valid = 0;
}

void disk_cache::slot::reload(qword aligned_lba){
	assert(objlock.is_locked() && objlock.is_exclusive());
	assert(aligned_lba == page_lba(aligned_lba));
	flush();
	lba_base = aligned_lba;
	auto res = ide.read(lba_base,phy_page,PAGE_SIZE);
	if (!res){
		//TODO handle failure
		bugcheck("disk read failed @ LBA %x",lba_base);
	}
	valid = 0xFF;
	timestamp = timer.running_time();
	objlock.downgrade();
}

void disk_cache::slot::load(qword lba,byte count){
	assert(objlock.is_locked() && objlock.is_exclusive());
	assert(lba_base == page_lba(lba));
	//non-0xFF page must come from previous writes
	if (valid == 0xFF){
		objlock.downgrade();
		return;
	}
	
	assert(dirty);
	//just write dirty pages and read back
	reload(lba_base);
	//downgrade in reload
}

void disk_cache::slot::store(qword lba,byte count){
	assert(objlock.is_locked() && objlock.is_exclusive());
	auto aligned_lba = page_lba(lba);
	if (aligned_lba != lba_base){
		flush();
		lba_base = aligned_lba;
	}
	unsigned off = lba - lba_base;
	for (unsigned i = 0;i < count;++i){
		assert(off + i < 8);
		byte mask = 1 << (off + i);
		dirty |= mask;
	}
	valid |= dirty;
	timestamp = timer.running_time();
}

void* disk_cache::slot::data(qword lba) const{
	assert(valid);
	assert(lba_base == page_lba(lba));
	return (byte*)access + (lba - lba_base)*SECTOR_SIZE;
}

disk_cache::disk_cache(word count) : slot_count(count), \
	table((slot*)operator new(sizeof(slot)*count))
{
	auto va = vm.reserve(0,slot_count);
	if (!va)
		bugcheck("disk_cache cannot reserve %x",(qword)slot_count);
	for (auto i = 0;i < slot_count;++i){
		new (table + i) slot(va + i*PAGE_SIZE);
	}
	dbgprint("disk_cache with %d slots @ %p",slot_count,va);
}

byte disk_cache::count(qword lba){
	auto top = align_up(lba,PAGE_SIZE/SECTOR_SIZE);
	return (top == lba) ? PAGE_SIZE/SECTOR_SIZE : top - lba;
}

disk_cache::slot* disk_cache::get(qword lba,byte count,bool write){
	auto aligned_lba = page_lba(lba);
	if (align_up(lba + count,PAGE_SIZE/SECTOR_SIZE) - aligned_lba != (PAGE_SIZE/SECTOR_SIZE)){
		bugcheck("disk_cache::slot::get bad param %x,%d",lba,count);
		return nullptr;
	}
	byte times = 0;
	do{
		//lock_guard<rwlock> guard(cache_guard);
		slot* target = nullptr;
		for (unsigned index = 0;index < slot_count;++index){
			auto& cur = table[index];
			if (cur.match(aligned_lba)){
				cur.lock(rwlock::EXCLUSIVE);
				if (cur.match(aligned_lba)){
					if (write)
						cur.store(lba,count);
					else
						cur.load(lba,count);
					return &cur;
				}
				cur.unlock();
			}
			if (cur.try_lock(rwlock::EXCLUSIVE)){
				if (!target || target->timestamp > cur.timestamp)
					target = &cur;
				cur.unlock();
			}
		}
		if (target && target->try_lock(rwlock::EXCLUSIVE)){
			if (write)
				target->store(lba,count);
			else
				target->reload(aligned_lba);
			return target;
		}
		if (!times)
			thread::sleep(0);
		else
			slot_guard.wait();
	}while(++times);
	bugcheck("disk_cache::get failed");
}

void disk_cache::relax(slot* ptr){
	assert(ptr && ptr - table < slot_count);
	ptr->unlock();
	if (!ptr->is_locked()){
		interrupt_guard<void> ig;
		slot_guard.signal_all();
		slot_guard.reset();
	}
}


/*
byte disk_cache::read(qword lba,byte count,void* buffer){
	//handles misaligned sectors
	auto top = lba + count;
	while(lba < top){
		auto slice_top = align_up(lba + 1,PAGE_SIZE/SECTOR_SIZE);
		slice_top = min(slice_top,top);
		auto slice_len = slice_top - lba;
		assert(slice_len && slice_len <= 8);
		auto block = get(lba,slice_len,false);
		if (block == nullptr)
			break;
		dbgprint("cache %d for LBA %x",block - table,block->lba_base);
		memcpy(buffer,block->data(lba),slice_len*SECTOR_SIZE);
		relax(block);
		buffer = (byte*)buffer + slice_len*SECTOR_SIZE;
		lba = slice_top;
	}
	return lba + count - top;
}

byte disk_cache::write(qword lba,byte count,const void* buffer){
	dbgprint("disk_cache::write not implemented");
	return 0;
}
*/