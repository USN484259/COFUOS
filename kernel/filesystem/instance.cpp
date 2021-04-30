#include "instance.hpp"
#include "dev/include/disk_interface.hpp"
#include "lock_guard.hpp"
#include "string.hpp"

using namespace UOS;

file_instance::file_instance(exfat& f,literal&& str,folder_instance* top) : \
	parent(top), name(move(str)), clusters(f,top == nullptr)
{
	objlock.lock();
}

file_instance::~file_instance(void){
	assert(objlock.is_locked() && objlock.is_exclusive());
	assert(ref_count == 0);
	assert(parent);
	parent->close(this);
	objlock.unlock();
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
	guard.drop();
	delete this;
}

qword file_instance::get_lba(qword offset){
	auto cluster = clusters.get(offset);
	if (cluster){
		offset = (offset % clusters.host().cluster_size()) / SECTOR_SIZE;
		return clusters.host().lba_of_cluster(cluster) + offset;
	}
	return 0;
}

folder_instance::folder_instance(exfat& f,literal&& str,folder_instance* top) : \
	file_instance(f,move(str),top)
{
	attribute = FOLDER;
}

folder_instance::~folder_instance(void){
	IF_assert;
	assert(objlock.is_locked() && objlock.is_exclusive());
	assert(file_table.empty());
}

struct record{
	byte data[0x20];
};
static_assert(sizeof(record) == 0x20,"exfat file record size mismatch");

file_instance* folder_instance::open(const span<char>& str){
	lock_guard<rwlock> guard(objlock);
	auto it = file_table.find(str);
	if (it != file_table.end()){
		(*it)->acquire();
		return *it;
	}
	file_instance* instance = nullptr;
	disk_interface::slot* slot = nullptr;
	dword index = 0;
	auto hash_value = exfat::name_hash(str);
	constexpr dword record_per_sector = SECTOR_SIZE/sizeof(record);
	vector<record> entrance;
	do{
		//#error stack overflow on large cluster size
		//FAT32::record buffer[FAT32::cluster_size() / sizeof(FAT32::record)];
		auto lba = get_lba(index*sizeof(record));
		if (lba == 0)
			bugcheck("exfat folder corrupted %x",(qword)index);
		if (slot)
			dm.relax(slot);
		slot = dm.get(lba,1,false);
		if (slot == nullptr){
			//TODO handle bad sectors
			bugcheck("exfat failed to read folder @ %x,%x",(qword)index,lba);
		}
		auto buffer = (record*)slot->data(lba);

		do{
		//for (auto& record : buffer){
			auto& rec = buffer[index % record_per_sector];
			if (rec.data[0] == 0)
				goto done;
			if (rec.data[0] > 0x80){
				switch(rec.data[0]){
					case 0x85:
						if (rec.data[1] >= 2){
							entrance.clear();
							entrance.push_back(rec);
							continue;
						}
						break;
					case 0xC0:
						if (hash_value == (((word)rec.data[5] << 8) | rec.data[4])){
							entrance.push_back(rec);
							continue;
						}
						break;
					case 0xC1:
					{	
						entrance.push_back(rec);
						dword count = entrance.front().data[1];
						if (1 + count == entrance.size() && index >= count){
							instance = imp_open(index - count,str,entrance.data());
							if (instance){
								file_table.insert(instance);
								++ref_count;
								goto done;
							}
						}
					}
				}
			}
			entrance.clear();
		}while(++index % record_per_sector);
	}while(true);
done:
	if (slot)
		dm.relax(slot);
	return instance;
}

struct entrance{
	byte magic_0;
	byte entrance_size;
	word checksum;
	word attributes;
	word reserved[0x0D];
	byte magic_1;
	byte alloc_stat;
	byte reserved_1;
	byte name_size;
	word name_hash;
	word reserved_2;
	qword valid_size;
	dword reserved_3;
	dword first_cluster;
	qword alloc_size;
};
struct name_part{
	byte type;
	byte zero;
	word str[0x0F];
};
static_assert(sizeof(entrance) == 0x40,"exfat file entrance size mismatch");
static_assert(sizeof(name_part) == 0x20,"exfat file name part size mismatch");

file_instance* folder_instance::imp_open(dword index,const span<char>& str,const void* buffer){
	auto rec = (const entrance*)buffer;
	dword limit = 0x20*(1 + rec->entrance_size);
	word sum = 0;
	for (unsigned i = 0;i < limit;++i){
		if (i == 2 || i == 3)
			continue;
		sum = (((sum & 1) ? 0x8000 : 0) | (sum >> 1)) + ((const byte*)buffer)[i];
	}
	if (rec->checksum != sum)
		return nullptr;

	if (rec->name_size != str.size())
		return nullptr;
	
	char name[rec->name_size];
	auto name_list = (const name_part*)(rec + 1);
	for (unsigned i = 0;i < rec->name_size;++i){
		if (2 + i/0xF > rec->entrance_size)
			return nullptr;
		auto ch = name_list[i/0xF].str[i%0xF];
		if (ch == 0 || ch & 0xFF00)
			return nullptr;
		name[i] = ch & 0xFF;
	}
	if (!exfat::name_equal(span<char>(name,rec->name_size),str))
		return nullptr;

	// file found, create instance
	file_instance* instance;
	if (rec->attributes & FOLDER){
		if (rec->valid_size != rec->alloc_size)
			return nullptr;
		instance = new folder_instance(clusters.host(),literal(name,name + rec->name_size),this);
	}
	else{
		instance = new file_instance(clusters.host(),literal(name,name + rec->name_size),this);
	}
	instance->valid_size = rec->valid_size;
	instance->rec_index = index;
	instance->name_hash = rec->name_hash;
	instance->attribute = rec->attributes;
	instance->clusters.assign(rec->first_cluster,rec->alloc_size,rec->alloc_stat & 2);
	instance->objlock.unlock();
	return instance;
}

void folder_instance::as_root(dword root_cluster){
	assert(objlock.is_locked() && objlock.is_exclusive() && parent == nullptr);
	attribute = FOLDER;
	clusters.assign(root_cluster,0,false);
	objlock.unlock();
}

void folder_instance::close(file_instance* ptr){
	assert(ptr->objlock.is_locked() && ptr->objlock.is_exclusive());
	assert(ptr->parent == this);
	lock_guard<rwlock> guard(objlock);
	auto it = file_table.find(ptr->name);
	assert(it != file_table.end() && *it == ptr);
	file_table.erase(it);
	assert(ref_count);
	if (--ref_count)
		return;
	guard.drop();
	delete this;
}
