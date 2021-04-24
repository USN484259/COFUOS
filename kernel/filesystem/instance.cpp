#include "instance.hpp"
#include "dev/include/disk_cache.hpp"
#include "lock_guard.hpp"
#include "string.hpp"

using namespace UOS;

file_instance::file_instance(FAT32& f,literal&& str,folder_instance* top,const FAT32::record* rec) : \
	parent(top), name(move(str)), clusters(f)
{
	if (rec){
		size = rec->size;
		attribute = rec->attrib;
		rec_size = rec->rec_size;
		rec_index = rec->rec_index;
		dword first_cluster = ((dword)rec->cluster_hi << 16) | rec->cluster_lo;
		clusters.set_head(first_cluster);
		// if (top){
		// 	lock_guard<rwlock> guard(objlock);
		// 	#error deadlock here
		// 	top->attach(this);
		// }
	}
}

file_instance::~file_instance(void){
	assert(objlock.is_locked() && objlock.is_exclusive());
	assert(ref_count == 0);
	assert(parent);
	parent->detach(this);
}

void file_instance::acquire(void){
	lock_guard<rwlock> guard(objlock);
	++ref_count;
}

void file_instance::relax(void){
	lock_guard<rwlock> guard(objlock);
	assert(ref_count);
	if (--ref_count)
		return;
	delete this;
}

qword file_instance::get_lba(dword offset){
	auto cluster = clusters.get(offset);
	if (cluster)
		return clusters.fs.lba_from_cluster(cluster);
	return 0;
}

folder_instance::folder_instance(FAT32& f,literal&& str,folder_instance* top,const FAT32::record* rec) \
	: file_instance(f,move(str),top,rec) {}

folder_instance::~folder_instance(void){
	IF_assert;
	assert(objlock.is_locked() && objlock.is_exclusive());
	assert(file_table.empty());
}

file_instance* folder_instance::open(const span<char>& str){
	lock_guard<rwlock> guard(objlock);
	auto it = file_table.find(str);
	if (it != file_table.end()){
		(*it)->acquire();
		return *it;
	}
	file_instance* instance = nullptr;
	disk_cache::slot* slot = nullptr;
	vector<FAT32::record> long_name;
	dword off = 0;
	do{
		//#error stack overflow on large cluster size
		//FAT32::record buffer[FAT32::cluster_size() / sizeof(FAT32::record)];
		auto lba = get_lba(off);
		if (lba == 0)
			bugcheck("FAT32_record corrupted %x",(qword)off);
		if (slot)
			dm.relax(slot);
		slot = dm.get(lba,1,false);
		if (slot == nullptr){
			//TODO handle bad sectors
			bugcheck("failed to read FAT32_record @ %x,%x",(qword)off,lba);
		}
		auto buffer = (FAT32::record*)slot->data(lba);
		for (unsigned i = 0;i < SECTOR_SIZE / sizeof(FAT32::record);++i){
		//for (auto& record : buffer){
			auto& record = buffer[i];
			if (0x0F == (record.attrib & 0x0F)){
				long_name.push_back(record);
				continue;
			}
			switch(record.type){
				case 0:		//end of table
					goto done;
				case 0xE5:	//deleted entrance
					long_name.clear();
					continue;
			}
			auto name = FAT32::resolve_name(record,long_name);
			
			if (!name.empty() && equal()(name,str)){
				record.rec_index = i + off/sizeof(FAT32::record);
				record.rec_size = long_name.empty() ? 1 : long_name.size();
				
				if (record.attrib & FOLDER)
					instance = new folder_instance(clusters.fs,literal(name.begin(),name.end()),this,&record);
				else
					instance = new file_instance(clusters.fs,literal(name.begin(),name.end()),this,&record);

				//attach to this folder
				file_table.insert(instance);
				++ref_count;
				

				//file_table.insert(instance);
				//acquire();
				goto done;
			}
			long_name.clear();
		}
		off += SECTOR_SIZE;
	}while(true);
done:
	if (slot)
		dm.relax(slot);
	return instance;
}

void folder_instance::flush(file_instance* ptr){
	assert(ptr->objlock.is_locked());
	assert(ptr->parent == this);
	if (ptr->state & DIRTY)
		;
	else
		return;
	lock_guard<rwlock> guard(objlock);
	imp_flush(ptr);
}

void folder_instance::attach(file_instance* ptr){
	assert(ptr->objlock.is_locked() && ptr->objlock.is_exclusive());
	assert(ptr->parent == this);
	lock_guard<rwlock> guard(objlock);
	//assert(file_table.find(ptr) == file_table.end());
	assert(file_table.find(ptr->name) == file_table.end());
	file_table.insert(ptr);
	//acquire();
	++ref_count;
	ptr->state |= DIRTY;
}

void folder_instance::detach(file_instance* ptr){
	assert(ptr->objlock.is_locked() && ptr->objlock.is_exclusive());
	assert(ptr->parent == this);
	lock_guard<rwlock> guard(objlock);
	if (ptr->state & DIRTY)
		;
	else
		imp_flush(ptr);
	auto it = file_table.find(ptr->name);
	assert(it != file_table.end() && *it == ptr);
	file_table.erase(it);
	//relax();
	assert(ref_count);
	if (--ref_count)
		return;
	delete this;
}

void folder_instance::imp_flush(file_instance* ptr){
	assert(objlock.is_locked() && objlock.is_exclusive());
	assert(ptr->objlock.is_locked() && ptr->parent == this);
	//assert(file_table.find(ptr) != file_table.end());
	assert(file_table.find(ptr->name) != file_table.end());
	//TODO
	bugcheck("updating FAT32_record not implemented");

}

