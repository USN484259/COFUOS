#include "exfat.hpp"
#include "lock_guard.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"
#include "dev/include/disk_interface.hpp"

using namespace UOS;

exfat::exfat(unsigned count){
	this_core core;
	auto this_process = core.this_thread()->get_process();
	qword args[4] = { reinterpret_cast<qword>(this),count };
	th_init = this_process->spawn(thread_init,args);
	assert(th_init);
	th_init->set_priority(scheduler::kernel_priority + 2);
}

void exfat::stop(void){
	bugcheck("exfat::stop not implemented");
}

void exfat::task(file* f){
	// TODO check 'writable' bit
	workers.put(f);
}

qword exfat::lba_of_fat(dword sector) const{
	constexpr dword element_per_sector = SECTOR_SIZE/sizeof(dword);
	auto limit = align_up(cluster_count + 2,element_per_sector) / element_per_sector;
	return (sector < limit) ? table + sector : (-1);
}

qword exfat::lba_of_cluster(dword cluster) const{
	assert(cluster >= 2 && cluster < 0xFFFFFFF7);
	if (cluster - 2 >= cluster_count)
		return (-1);
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
		assert(slot);
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
		assert(slot);
		buffer = (const byte*)slot->data(self->base + sec);
	}
	if (sum != *(const dword*)buffer)
		bugcheck("exFAT checksum mismatch (%x,%x)",(qword)sum,(qword)*(const dword*)buffer);
	
	// parse root directory and get allocation bitmap
	assert(root);
	byte fat_index = (root & 0x80000000) ? 1 : 0;
	root &= 0x7FFFFFFF;
	dm.relax(slot);
	auto lba = self->lba_of_cluster(root);
	slot = dm.get(lba,1,false);
	assert(slot);
	buffer = (const byte*)slot->data(lba);
	self->bitmap = 0;
	for (auto ptr = buffer;ptr - buffer < SECTOR_SIZE;ptr += 0x20){
		if (ptr[0] == 0)
			break;
		if (ptr[0] == 0x81 && fat_index == (ptr[1] & 1)){
			auto length = *(const qword*)(ptr + 0x18);
			if (align_up(self->cluster_count,8)/8 <= length){
				self->bitmap = self->lba_of_cluster(*(const dword*)(ptr + 0x14));
			}
			break;
		}
	}
	if (self->bitmap == 0)
		bugcheck("exFAT missing allocation bitmap");
	dm.relax(slot);
	// construct root folder instance
	self->root = new folder_instance(*self,literal(),nullptr);
	self->root->as_root(root);

	self->workers.launch(self,cnt);
	this_core core;
	thread::kill(core.this_thread());
}

void cluster_chain::assign(dword head,qword sz,bool linear_block){
	lock_guard<rwlock> guard(objlock);
	first_cluster = head;
	alloc_size = sz;
	linear = linear_block;
	lru_queue.clear();
	cache_line.clear();
}

dword cluster_chain::get(qword offset){
	const dword index = offset / host().cluster_size();
	lock_guard<rwlock> guard(objlock);
	if (first_cluster == 0)
		return 0;
	if (offset >= alloc_size && !root)
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
	for (auto& l : cache_line){
		assert(l.index != cur_line.index);
		if (l.index < index){
			cur_line = l;
			break;
		}
	}
	constexpr dword element_per_sector = SECTOR_SIZE/sizeof(dword);

	dword sector_index = 0;
	qword sector_lba = 0;
	disk_interface::slot* fat_sector = nullptr;

	while(cur_line.index < index){
		if (cur_line.cluster == 0)
			break;
		auto sector = cur_line.cluster / element_per_sector;
		auto offset = cur_line.cluster % element_per_sector;

		if (!fat_sector || sector_index != sector){
			if (fat_sector)
				dm.relax(fat_sector);
			sector_lba = host().lba_of_fat(sector);
			fat_sector = dm.get(sector_lba,1,false);
			if (fat_sector == nullptr){
				bugcheck("FIXME handle FAT read failure");
			}
			sector_index = sector;
		}
		auto next = ((dword*)fat_sector->data(sector_lba))[offset];
		if (next >= 0xFFFFFFF8)
			next = 0;
		++cur_line.index;
		cur_line.cluster = next;
	}
	if (fat_sector)
		dm.relax(fat_sector);
	//remove excess lines
	while(lru_queue.size() > limit){
		auto it = lru_queue.back();
		cache_line.erase(it);
		lru_queue.pop_back();
	}
	assert(lru_queue.size() == cache_line.size());
	return cur_line.index == index ? cur_line.cluster : 0;
}

bool cluster_chain::set(qword offset){
	bugcheck("cluster_chain::set not implemented");
}