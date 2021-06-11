#include "exfat.hpp"
#include "lock_guard.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"

using namespace UOS;

constexpr dword element_per_sector = SECTOR_SIZE/sizeof(dword);

class fat_reader{
	const exfat& host;
	dword sector_index = 0;
	qword sector_lba = 0;
	disk_interface::slot* fat_sector = nullptr;

	bool load(dword cluster){
		auto sector = cluster / element_per_sector;
		// auto offset = cluster % element_per_sector;
		if (!fat_sector || sector_index != sector){
			if (fat_sector)
				dm.relax(fat_sector,true);
			sector_lba = host.lba_of_fat(sector);
			if (sector_lba == 0){
				dbgprint("cluster overflow %x",sector);
				return false;
			}
			fat_sector = dm.get(sector_lba,1,false);
			if (fat_sector == nullptr){
				dbgprint("FAT read failure @ %x,%x",cluster,sector_lba);
				return false;
			}
			sector_index = sector;
		}
		return true;
	}

public:
	fat_reader(const exfat& fs) : host(fs) {}
	~fat_reader(void){
		if (fat_sector)
			dm.relax(fat_sector,true);
	}
	dword get(dword cluster){
		assert(cluster >= 2);
		if (!load(cluster))
			return 0;
		auto offset = cluster % element_per_sector;
		return ((const dword*)fat_sector->data(sector_lba))[offset];
	}
	bool put(dword tail,dword next){
		assert(tail >= 2 && next >= 2);

		if (!load(next))
			return false;
		auto offset = next % element_per_sector;
		auto ptr = (dword*)fat_sector->data(sector_lba);
		dm.upgrade(fat_sector,sector_lba,1);
		ptr[offset] = 0xFFFFFFFF;

		if (!load(tail))
			return false;
		offset = tail % element_per_sector;
		ptr = (dword*)fat_sector->data(sector_lba);
		if (ptr[offset] < 0xFFFFFFF8){
			dbgprint("Concatenating non-tail cluster %x",tail);
			return false;
		}
		dm.upgrade(fat_sector,sector_lba,1);
		ptr[offset] = next;
		return true;
	}
};

exfat::exfat(unsigned count){
	this_core core;
	auto this_process = core.this_thread()->get_process();
	qword args[4] = { reinterpret_cast<qword>(this),count };
	th_init = this_process->spawn(thread_init,args);
	assert(th_init);
	//th_init->set_priority(scheduler::kernel_priority + 2);
}

bool exfat::set_rw(bool force){
	if (0 != cmpxchg(&flags,(word)ALLOW_WRITE,(word)0)){
		// already rw || stopping
		return false;
	}
	auto block = dm.get(base,1,false);
	if (!block)
		return false;
	auto ptr = (byte*)block->data(base);
	volume_state = (ptr[0x6A] & ALLOW_WRITE);
	if ((0 == volume_state) || force){
		dm.upgrade(block,base,1);
		ptr[0x6A] |= ALLOW_WRITE;
	}
	dm.relax(block,true);
	return 0 == volume_state;
}

bool exfat::stop(void){
	if (ALLOW_WRITE != cmpxchg(&flags,(word)(ALLOW_WRITE | STOPPING),(word)ALLOW_WRITE)){
		// not rw || already stopped
		return false;
	}
	workers.stop();
	dm.flush();
	if (0 == volume_state){
		auto block = dm.get(base,1,false);
		if (!block){
			return false;
		}
		auto ptr = (byte*)block->data(base);
		dm.upgrade(block,base,1);
		ptr[0x6A] &= (byte)~ALLOW_WRITE;
		dm.relax(block,true);
	}
	return true;
}

void exfat::task(file* f){
	if (is_running()){
		if (f->command != file::COMMAND_WRITE || is_rw()){
			workers.put(f);
			return;
		}
	}
	interrupt_guard<spin_lock> guard(f->objlock);
	lock_or(&f->iostate,(word)OP_FAILURE);
	f->length = 0;
	f->command = 0;
	guard.drop();
	f->notify();
}

qword exfat::lba_of_fat(dword sector) const{
	auto limit = align_up(cluster_count + 2,element_per_sector) / element_per_sector;
	return (sector < limit) ? table + sector : 0;
}

qword exfat::lba_of_cluster(dword cluster) const{
	assert(cluster >= 2 && cluster < 0xFFFFFFF7);
	if (cluster - 2 >= cluster_count)
		return 0;
	return heap + (cluster - 2) * (cluster_size() / SECTOR_SIZE);
}

dword exfat::parse_header(const void* ptr){
	struct exfat_header{
		byte jmp_boot[3];
		char fs_name[8];
		byte zero[0x35];
		qword base;
		qword limit;
		dword fat_offset;
		dword fat_length;
		dword cluster_offset;
		dword cluster_count;
		dword root_dir;
		dword serial_number;
		word fs_version;
		word flags;
		byte sector_shift;
		byte cluster_shift;
		byte fat_count;
		byte drive_select;
		byte use_percent;
		byte reserved[7];
		byte boot_code[0x186];
		word signature;
	};
	static_assert(sizeof(exfat_header) == SECTOR_SIZE,"exfat header size mismatch");
	auto header = (const exfat_header*)ptr;
	if (header->signature != 0xAA55)
		return 0;
	if (8 != match(header->fs_name,"EXFAT   ",8,equal_to<char>()))
		return 0;
	for (unsigned i = 0;i < 0x35;++i){
		if (header->zero[i] != 0)
			return 0;
	}
	if (header->fs_version != 0x0100)
		return 0;
	if (header->sector_shift != 9)
		return 0;
	if (header->base != base)
		return 0;
	if (top && header->base + header->limit > top)
		return 0;
	top = header->base + header->limit;

	if (header->fat_offset < 24 \
		|| header->fat_length == 0 \
		|| header->cluster_offset == 0 \
		|| header->cluster_count == 0)
			return 0;
	cluster_count = header->cluster_count;
	cluster_shift = 9 + header->cluster_shift;
	table = base + header->fat_offset;
	heap = base + header->cluster_offset;
	switch(header->fat_count){
		case 2:
			if (header->flags & 1)
				table += header->fat_length;
			break;
		case 1:
			if (0 == (header->flags & 1))
				break;
		default:
			return 0;
	}
	if (align_up((2 + cluster_count)*sizeof(dword),SECTOR_SIZE)/SECTOR_SIZE > header->fat_length)
		return 0;
	if (table + header->fat_length*header->fat_count > heap)
		return 0;
	if (heap + cluster_count*((qword)1 << header->cluster_shift) > top)
		return 0;
	if (header->root_dir < 2)
		return 0;
	return ((header->flags & 1) ? 0x80000000 : 0) | header->root_dir;
}

void exfat::thread_init(qword arg,qword cnt,qword,qword){
	auto self = reinterpret_cast<exfat*>(arg);
	dbgprint("initializing filesystem...");

	disk_interface::slot* slot = nullptr;
	const byte* buffer;
	dword root = 0;
	self->base = 0;
	do{
		slot = dm.get(self->base,1,false);
		if (!slot){
			bugcheck("failed reading exFAT header");
		}
		buffer = (const byte*)slot->data(self->base);
		root = self->parse_header(buffer);
		if (root)
			break;
		if (self->base)
			bugcheck("failed to locate exFAT partition %x",self->base);
		if (*(const word*)(buffer + 0x1FE) != 0xAA55)
			bugcheck("boot partition bad signature %x",(qword)*(const word*)(buffer + 0x1FE));
		// parse MBR
		for (auto ptr = buffer + 0x1BE;ptr < buffer + 0x1FE;ptr += 0x10){
			switch(ptr[0]){
				case 0x80:
					if (ptr[4] == 0x07){
						self->base = *(const dword*)(ptr + 8);
						self->top = self->base + *(const dword*)(ptr + 0x0C);
						break;
					}
				case 0:
					continue;
			}
			break;
		}
		if (!self->base)
			bugcheck("failed to locate exFAT partition in MBR");
		dm.relax(slot);
	}while(true);

	// calculate boot area checksum
	dword sum = 0;
	for (unsigned sec = 0;sec < 11;){
		for (unsigned i = 0;i < SECTOR_SIZE;++i){
			if (sec == 0){
				switch(i){
					case 0x6A:
					case 0x6B:
					case 0x70:
						continue;
				}
			}
			sum = (((sum & 1) ? 0x80000000 : 0) | (sum >> 1)) + buffer[i];
		}
		dm.relax(slot);
		++sec;
		slot = dm.get(self->base + sec,1,false);
		if (!slot){
			bugcheck("failed reading exFAT boot area %d",sec);
		}
		buffer = (const byte*)slot->data(self->base + sec);
	}
	if (sum != *(const dword*)buffer)
		bugcheck("exFAT checksum mismatch (%x,%x)",(qword)sum,(qword)*(const dword*)buffer);
	
	// parse root directory and get allocation bitmap

	byte fat_index = (root & 0x80000000) ? 1 : 0;
	root &= 0x7FFFFFFF;
	if (root == 0)
		bugcheck("failed locating root directory");
	self->bitmap = 0;
	dword cur_cluster = root;
	dword index = 0;
	do{
		auto lba = self->lba_of_cluster(cur_cluster);
		if (lba == 0)
			break;
		dm.relax(slot);
		slot = dm.get(lba,1,false);
		if (!slot){
			break;
		}
		buffer = (const byte*)slot->data(lba);

		do{
			if (buffer[0] == 0)
				goto done;
			if (buffer[0] == 0x81 && fat_index == (buffer[1] & 1)){
				auto length = *(const qword*)(buffer + 0x18);
				if (align_up(self->cluster_count,8)/8 <= length){
					self->bitmap = self->lba_of_cluster(*(const dword*)(buffer + 0x14));
				}
			}
		}while(buffer += record_size,++index % record_per_sector);
		auto sector = cur_cluster / element_per_sector;
		auto offset = cur_cluster % element_per_sector;
		dm.relax(slot);
		lba = self->lba_of_fat(sector);
		if (lba == 0){
			break;
		}
		slot = dm.get(lba,1,false);
		if (!slot){
			break;
		}
		auto fat = (const dword*)slot->data(lba);
		cur_cluster = fat[offset];
	}while(cur_cluster < 0xFFFFFFF8);
	bugcheck("failed reading exFAT root directory");
done:
	dm.relax(slot);
	if (self->bitmap == 0 || self->bitmap >= self->top)
		bugcheck("exFAT invalid allocation bitmap @ %x",(qword)self->bitmap);
	// construct root folder instance
	record rec = {0};
	rec.first_cluster = root;
	rec.valid_size = rec.alloc_size = index*record_size;
	rec.attributes = FOLDER;
	self->root = new folder_instance(*self,&rec);
	//self->root->as_root(root);

	self->workers.launch(self,cnt);
	this_core core;
	thread::kill(core.this_thread());
}

exfat::allocator::allocator(exfat& f) : fs(f){
	f.bmp_lock.lock();
}

exfat::allocator::~allocator(void){
	if (block)
		dm.relax(block,true);
	fs.bmp_lock.unlock();
}

dword exfat::allocator::get(void){
	auto pos = fs.bmp_last_index;
	auto initial = align_down(pos,bit_per_sector);
	do{
		auto sector = pos / bit_per_sector;
		auto lba = fs.bitmap + sector;
		if (!block || block_lba != lba){
			if (block)
				dm.relax(block,true);
			block = dm.get(lba,1,false);
			if (!block){
				dbgprint("Failed reading allocation bitmap @ %x",lba);
				return 0;
			}
			block_lba = lba;
		}
		auto ptr = (qword*)block->data(lba);
		qword data = ptr[(pos % bit_per_sector)/64];
		for(;pos < fs.cluster_count && pos % bit_per_sector;++pos){
			if (0 == (pos % 64)){
				data = ptr[(pos % bit_per_sector)/64];
			}
			qword mask = (qword)1 << (pos % 64);
			if (0 == (data & mask)){
				dm.upgrade(block,lba,1);
				data |= mask;
				ptr[(pos % bit_per_sector)/64] = data;
				fs.bmp_last_index = pos;
				return pos + 2;
			}
		}
		if (pos >= fs.cluster_count)
			pos = 0;
	}while(pos != initial);
	return 0;
}

bool exfat::allocator::put(dword cluster){
	assert(cluster >= 2);
	auto sector = (cluster - 2) / bit_per_sector;
	auto offset = (cluster - 2) % bit_per_sector;
	auto lba = fs.bitmap + sector;
	if (!block || block_lba != lba){
		if (block)
			dm.relax(block,true);
		block = dm.get(lba,1,false);
		if (!block){
			dbgprint("Failed reading allocation bitmap @ %x",lba);
			return 0;
		}
		block_lba = lba;
	}
	auto ptr = (qword*)block->data(lba);
	qword data = ptr[offset / 64];
	qword mask = (qword)1 << (offset % 64);
	if (data & mask){
		dm.upgrade(block,lba,1);
		data &= (~mask);
		ptr[offset / 64] = data;
		return true;
	}
	dbgprint("exfat double release %x",cluster);
	return false;
}

dword cluster_chain::get(dword index){
	lock_guard<rwlock> guard(objlock);
	if (first_cluster == 0)
		return 0;
	if (linear)
		return first_cluster + index;
	if (index == 0)
		return first_cluster;
	for (auto it = lru_queue.begin();it != lru_queue.end();++it){
		if ((*it)->index == index){
			if (it == lru_queue.begin())
				return (*it)->cluster;
			auto node = lru_queue.get_node(it);
			lru_queue.put_node(lru_queue.begin(),node);
			return node->payload->cluster;
		}
	}
	// not in cache, read from FAT
	line cur_line = {0,first_cluster};
	auto it = cache_line.begin();
	for (;it != cache_line.end();++it){
		assert(it->index != cur_line.index);
		if (it->index < index){
			cur_line = *it;
			break;
		}
	}
	{
		fat_reader fat(host());
		while(cur_line.index < index){
			auto next = fat.get(cur_line.cluster);
			if (next == 0 || next >= 0xFFFFFFF8)
				return 0;
			++cur_line.index;
			cur_line.cluster = next;
		}
	}
	assert(cur_line.index == index);

	//insert into cache_line
	it = cache_line.insert(it,cur_line);
	lru_queue.push_front(it);

#ifndef NDEBUG
	{
		// check cache_line
		dword prev = 0;
		for (auto& l : cache_line){
			assert(l.index);
			assert(prev == 0 || l.index < prev);
			prev = l.index;
		}
	}
#endif

	//remove excess lines
	while(lru_queue.size() > limit){
		auto it = lru_queue.back();
		cache_line.erase(it);
		lru_queue.pop_back();
	}
	assert(lru_queue.size() == cache_line.size());

	return cur_line.cluster;
}

bool cluster_chain::expand(dword index){
	lock_guard<rwlock> guard(objlock);
	if (linear){
		dbgprint("expanding linear cluster chain not supported");
		return false;
	}
	line tail = {0, first_cluster};
	if (!cache_line.empty()){
		tail = cache_line.front();
	}
	if (tail.index >= index)
		return true;
	// follow cluster chain and find tail cluster
	fat_reader fat(host());
	do{
		auto next = fat.get(tail.cluster);
		if (next == 0)
			return false;
		if (next >= 0xFFFFFFF8)
			break;
		++tail.index;
		tail.cluster = next;
	}while(tail.index < index);
	if (tail.index >= index)
		return true;
	// tail is the last cluster

	//allocate clusters
	exfat::allocator alloc(host());

	while(tail.index < index){
		auto next = alloc.get();
		if (next == 0){
			// no free cluster
			return false;
		}
		if (!fat.put(tail.cluster,next))
			return false;
		if (tail.index == 0){
			assert(first_cluster == 0);
			first_cluster = next;
		}
		tail.cluster = next;
		++tail.index;
	}
	return true;
}