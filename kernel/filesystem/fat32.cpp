#include "fat32.hpp"
#include "instance.hpp"
#include "dev/include/disk_cache.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"
#include "lock_guard.hpp"
#include "constant.hpp"
#include "sysinfo.hpp"

using namespace UOS;

void FAT32::cluster_cache::set_head(dword head){
	lock_guard<rwlock> guard(objlock);
	first_cluster = head;
	lru_queue.clear();
	cache_line.clear();
}

dword FAT32::cluster_cache::get(dword offset){
	dword index = offset / fs.get_cluster_size();
	lock_guard<rwlock> guard(objlock);
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
	//not in cache, read from FAT
	line cur_line = {0,first_cluster};
	for (auto& cur : cache_line){
		assert(cur.index != index);
		if (cur.index < index){
			cur_line = cur;
			break;
		}
	}
	constexpr dword element_per_sector = SECTOR_SIZE/sizeof(dword);
	
	dword sector_index = 0;
	qword sector_lba = 0;
	disk_cache::slot* fat_sector = nullptr;
	
	while(cur_line.index < index){
		if (cur_line.cluster == 0)
			break;
		auto sector = cur_line.cluster / element_per_sector;
		auto offset = cur_line.cluster % element_per_sector;

		if (!fat_sector || sector_index != sector){
			if (fat_sector)
				dm.relax(fat_sector);
			sector_lba = fs.lba_from_fat(sector);
			fat_sector = dm.get(sector_lba,1,false);
			if (fat_sector == nullptr){
				bugcheck("FIXME handle FAT read failure");
			}
			sector_index = sector;
		}
		auto next = ((dword*)fat_sector->data(sector_lba))[offset];
		if (next >= 0x0FFFFFF8)
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

void FAT32::block_pool::put(dword base,dword count){
	lock_guard<rwlock> guard(objlock);
	pool.push_back(block{base,count});
	total_count += count;
}

dword FAT32::block_pool::get(dword count){
	lock_guard<rwlock> guard(objlock);
	dword result = 0;
	for (auto it = pool.begin();it != pool.end();++it){
		if (it->count > count){
			it->count -= count;
			result = it->base + it->count;
			break;
		}
		if (it->count == count){
			result = it->base;
			pool.erase(it);
			break;
		}
	}
	if (result){
		assert(total_count >= count);
		total_count -= count;
	}
	return result;
}

dword FAT32::get_cluster_size(void){
	return cluster_size*SECTOR_SIZE;
}

qword FAT32::lba_from_fat(dword fat_index,bool table_index){
	return table[table_index] + fat_index;
}

qword FAT32::lba_from_cluster(dword cluster){
	assert(cluster >= 2);
	return data + cluster_size*(cluster - 2);
}

string FAT32::resolve_name(const record& rec,const vector<record>& long_name){
	string name;
	while(!long_name.empty()){
		//process long name
		if (0 == (long_name.front().type & 0x40))
			break;
		auto it = long_name.end();
		dword count = 1;
		static const byte pos[] = {1,3,5,7,9,0x0E,0x10,0x12,0x14,0x16,0x18,0x1C,0x1E};
		do{
			--it;
			if (it->attrib != 0x0F || (it->type & 0x3F) != count){
				count = 0;
				break;
			}
			auto block = (const char*)(&*it);
			for (auto off : pos){
				if (block[off + 1] != 0){
					count = 0;
					break;
				}
				if (block[off] == 0){
					if (it != long_name.begin())
						count = 0;
					break;
				}
				name.push_back(block[off]);
			}
			if (count == 0)
				break;
			++count;
		}while(it != long_name.begin());
		if (count != long_name.size())
			break;
		//valid long name, return
		return name;
	}
	name.clear();
	name.append(rec.name,rec.name + 8);
	while(!name.empty() && name.back() == ' ')
		name.pop_back();
	name.push_back('.');
	name.append(rec.name + 8,rec.name + 11);
	while(name.back() == ' ')
		name.pop_back();
	return name;
}

FAT32::FAT32(unsigned th_cnt) : workers(*this){
	this_core core;
	auto this_process = core.this_thread()->get_process();
	qword args[4] = { reinterpret_cast<qword>(this),th_cnt };
	th_init = this_process->spawn(thread_init,args);
	assert(th_init);
	th_init->set_priority(scheduler::kernel_priority + 2);

}

void FAT32::wait(void){
	th_init->wait();
}

void FAT32::task(file* f){
	workers.put(f);
}

void FAT32::thread_init(qword arg,qword th_cnt,qword,qword){
	auto self = reinterpret_cast<FAT32*>(arg);
	dbgprint("initializing filesystem...");
	//read MBR
	auto slot = dm.get(0,1,false);
	assert(slot);
	auto buffer = (byte*)slot->data(0);
	assert(buffer);
	auto ptr = buffer + 0x1BE;
	for (unsigned i = 0;i < 4;ptr += 0x10,++i){
		if (*ptr != 0x80)
			continue;
		if (*(ptr + 4) != 0x0C)
			continue;
		if (*(dword*)(ptr + 8) == sysinfo->FAT_header)
			break;
	}
	if (ptr - buffer >= 0x1FE)
		bugcheck("failed parsing MBR");
	self->base = *(dword*)(ptr + 8);
	self->top = self->base + *(dword*)(ptr + 0x0C);
	dm.relax(slot);

	//read FAT32 header
	slot = dm.get(self->base,1,false);
	assert(slot);
	buffer = (byte*)slot->data(self->base);
	assert(buffer);
	bool res = false;
	do{
		if (*(word*)(buffer + 0x0B) != SECTOR_SIZE)
			break;
		self->cluster_size = buffer[0x0D];
		if (self->cluster_size != sysinfo->FAT_cluster)
			break;
		auto reserved_count = *(word*)(buffer + 0x0E);
		if (buffer[0x10] != 2)
			break;
		auto sector_count = *(dword*)(buffer + 0x20);
		auto fat_size = *(dword*)(buffer + 0x24);
		if (*(dword*)(buffer + 0x2C) != 2)
			break;
		if (*(word*)(buffer + 0x1FE) != 0xAA55)
			break;
		if (self->base + sector_count > self->top)
			break;
		self->top = min(self->top,self->base + sector_count);
		self->table[0] = self->base + reserved_count;
		if (self->table[0] != sysinfo->FAT_table)
			break;
		self->table[1] = self->table[0] + fat_size;
		self->data = self->table[1] + fat_size;
		if (self->data != sysinfo->FAT_data)
			break;
		res = true;
	}while(false);
	if (!res)
		bugcheck("failed parsing FAT32");
	dm.relax(slot);

	//read first sector of FAT
	slot = dm.get(self->table[0],1,false);
	assert(slot);
	auto cluster = (dword*)slot->data(self->table[0]);
	assert(cluster);
	self->eoc = cluster[1];
	auto limit = 2 + (self->top - self->data)/self->cluster_size;
	constexpr dword element_per_sector = SECTOR_SIZE/sizeof(dword);

	if (self->eoc < 0x0FFFFFF8 || align_up(limit,element_per_sector)/element_per_sector != (self->table[1] - self->table[0]))
		bugcheck("failed parsing FAT32");

	dword free_base = 0;
	for (dword it = 2;it < limit;++it){
		//scan through FAT & record free clusters
		auto index = it % element_per_sector;
		if (0 == index){
			dm.relax(slot);
			auto lba = self->table[0] + it / element_per_sector;
			slot = dm.get(lba,1,false);
			assert(slot);
			cluster = (dword*)slot->data(lba);
			assert(cluster);
		}
		if (cluster[index]){
			if (free_base){
				self->free_clusters.put(free_base,it - free_base);
				free_base = 0;
			}
		}
		else if (free_base == 0){
			free_base = it;
		}
	}
	if (free_base){
		self->free_clusters.put(free_base,limit - free_base);
	} 
	dm.relax(slot);

	dword total = (self->top - self->data);
	dword free = self->free_clusters.size();
	if (total <= free)
		bugcheck("failed parsing FAT32");
	
	char total_unit = 'K';
	dword total_val = total / 2;
	if (total_val >= 0x400){
		total_unit = 'M';
		total_val /= 0x400;
	}
	if (total_val >= 0x400){
		total_unit = 'G';
		total_val /= 0x400;
	}
	char used_unit = 'K';
	dword used_val = (total - free) / 2;
	if (used_val >= 0x400){
		used_unit = 'M';
		used_val /= 0x400;
	}
	if (used_val >= 0x400){
		used_unit = 'G';
		used_val /= 0x400;
	}
	dbgprint("FAT32	@%x\t%d%cB/%d%cB",self->base,used_val,used_unit,total_val,total_unit);

	record rec = {0};
	rec.cluster_lo = 2;
	rec.type = FOLDER;
	self->root = new folder_instance(*self,literal(),nullptr,&rec);

	self->workers.launch(th_cnt);

	this_core core;
	thread::kill(core.this_thread());
}