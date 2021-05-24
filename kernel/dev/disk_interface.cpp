#include "disk_interface.hpp"
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

disk_interface::slot::slot(qword va) : access((void*)va),phy_page(pm.allocate(PM::MUST_SUCCEED)){
	//TODO allocate phy_page within 4G range
	auto res = vm.assign(va,phy_page,1);
	if (!res)
		bugcheck("disk_interface::slot failed %x,%x",va,(qword)phy_page);
}

bool disk_interface::slot::match(qword aligned_lba) const{
	assert(aligned_lba == page_lba(aligned_lba));
	if (valid == 0)
		return false;
	return lba_base == aligned_lba;
}

bool disk_interface::slot::flush(void){
	assert(objlock.is_locked() && objlock.is_exclusive());
	if (dirty == 0){
		valid = 0;
		return true;
	}
	byte head = 0;
	for (byte i = 0;i <= 8;++i){
		byte mask = 1 << i;
		if (0 == (dirty & mask)){
			if (head != i){
				// [head , i - 1]
				auto res = ide.command(IDE::WRITE,lba_base + head,phy_page + SECTOR_SIZE*head,SECTOR_SIZE*(i - head));
				if (!res){
					dbgprint("disk write failed @ LBA %x",lba_base + head);
					return false;
				}
			}
			head = i + 1;
		}
	}
	dirty = valid = 0;
	return true;
}

bool disk_interface::slot::reload(qword aligned_lba){
	assert(is_locked());
	assert(aligned_lba == page_lba(aligned_lba));
	if (!flush())
		return false;
	lba_base = aligned_lba;
	auto res = ide.command(IDE::READ,lba_base,phy_page,PAGE_SIZE);
	if (!res){
		dbgprint("disk read failed @ LBA %x",lba_base);
		return false;
	}
	valid = 0xFF;
	timestamp = timer.running_time();
	return true;
}

bool disk_interface::slot::load(qword lba,byte count){
	assert(is_locked());
	assert(lba_base == page_lba(lba));
	//non-0xFF page must come from previous writes
	if (valid == 0xFF){
		return true;
	}
	
	assert(dirty);
	//just write dirty pages and read back
	return reload(lba_base);
}

bool disk_interface::slot::store(qword lba,byte count){
	assert(is_locked());
	auto aligned_lba = page_lba(lba);
	if (aligned_lba != lba_base){
		if (!flush())
			return false;
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
	return true;
}

void* disk_interface::slot::data(qword lba) const{
	assert(valid);
	assert(lba_base == page_lba(lba));
	return (byte*)access + (lba - lba_base)*SECTOR_SIZE;
}

disk_interface::disk_interface(word count) : slot_count(count), \
	table((slot*)operator new(sizeof(slot)*count))
{
	auto va = vm.reserve(0,slot_count);
	if (!va)
		bugcheck("disk_interface cannot reserve %x",(qword)slot_count);
	for (auto i = 0;i < slot_count;++i){
		new (table + i) slot(va + i*PAGE_SIZE);
	}
	dbgprint("disk_interface with %d slots @ %p",slot_count,va);
}

byte disk_interface::count(qword lba){
	auto top = align_up(lba,PAGE_SIZE/SECTOR_SIZE);
	return (top == lba) ? PAGE_SIZE/SECTOR_SIZE : top - lba;
}

disk_interface::slot* disk_interface::get(qword lba,byte count,bool write){
	const auto aligned_lba = page_lba(lba);
	if (align_up(lba + count,PAGE_SIZE/SECTOR_SIZE) - aligned_lba != (PAGE_SIZE/SECTOR_SIZE)){
		bugcheck("disk_interface::slot::get bad param %x,%d",lba,count);
		return nullptr;
	}
	bool res;
	byte times = 0;
	do{
		slot* target = nullptr;
		for (unsigned index = 0;index < slot_count;++index){
			auto& cur = table[index];
			if (cur.match(aligned_lba)){
				cur.lock();
				if (cur.match(aligned_lba)){
					if (write)
						res = cur.store(lba,count);
					else
						res = cur.load(lba,count);
					return res ? &cur : nullptr;
				}
				cur.unlock();
			}
			if (cur.try_lock()){
				if (!target || target->timestamp > cur.timestamp)
					target = &cur;
				cur.unlock();
			}
		}
		if (target && target->try_lock()){
			if (write)
				res = target->store(lba,count);
			else
				res = target->reload(aligned_lba);
			return res ? target : nullptr;
		}
		if (!times)
			thread::sleep(0);
		else
			slot_guard.wait();
	}while(++times);
	bugcheck("disk_interface::get failed");
}

void disk_interface::relax(slot* ptr){
	assert(ptr && ptr - table < slot_count);
	ptr->unlock();
	if (!ptr->is_locked()){
		interrupt_guard<void> ig;
		slot_guard.signal_all();
		slot_guard.reset();
	}
}
