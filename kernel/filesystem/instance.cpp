#include "instance.hpp"
#include "dev/include/disk_interface.hpp"
#include "lock_guard.hpp"
#include "string.hpp"

using namespace UOS;

file_instance::file_instance(exfat& fs,folder_instance* top,literal&& str,dword index,const exfat::record* file) : \
	parent(top), name(move(str)), valid_size(file->valid_size), alloc_size(file->alloc_size), \
	rec_index(index), name_hash(file->name_hash), attribute(file->attributes), \
	clusters(fs,file->first_cluster, file->alloc_stat & 2)
{
#ifdef FS_TEST
	dbgprint("new instance %s",name.c_str());
#endif
}

file_instance::~file_instance(void){
	assert(objlock.is_locked() && objlock.is_exclusive());
	assert(ref_count == 0);
	assert(parent);
#ifdef FS_TEST
	dbgprint("delete instance %s",name.c_str());
#endif
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

qword file_instance::get_lba(qword offset,bool expand){
	// if (parent && offset >= alloc_size)
	// 	return 0;
	auto index = offset / clusters.host().cluster_size();
	do{
		auto cluster = clusters.get(index);
		if (cluster){
			auto base = clusters.host().lba_of_cluster(cluster);
			if (base){
				offset = (offset % clusters.host().cluster_size()) / SECTOR_SIZE;
				return base + offset;
			}
			break;
		}
		if (expand && clusters.expand(index))
			;	// retry, should succeed
		else
			break;
	}while(true);
	return 0;
}

bool file_instance::set_size(qword vs,qword fs){
	assert(objlock.is_locked() && objlock.is_exclusive());
	if (valid_size != vs || alloc_size != fs){
		valid_size = vs;
		alloc_size = fs;
		return parent->update_size(this);
	}
	return true;
}

void file_instance::imp_get_path(string& str) const{
	lock_guard<rwlock> guard(objlock,rwlock::SHARED);
	if (parent == nullptr)
		return;
	parent->imp_get_path(str);
	str.push_back('/');
	str.append(name.begin(),name.end());	
}

void file_instance::get_path(string& str) const{
	str.clear();
	imp_get_path(str);
	if (str.empty())
		str.push_back('/');
}

folder_instance::folder_instance(exfat& f,folder_instance* top,literal&& str,dword index,const exfat::record* file) : \
	file_instance(f,top,move(str),index,file)
{
	assert(attribute & FOLDER);
	assert(alloc_size == valid_size);
	lock_guard<rwlock> guard(objlock);
	reader rec(*this);
	switch(rec.step(0)){
		case MEDIA_FAILURE:
		case FS_FAILURE:
			dbgprint("Failed walking through folder %s",name.c_str());
		case EOF_FAILURE:
			break;
		default:
			assert(false);
	}
	valid_size = min<qword>(valid_size,rec.get_index()*exfat::record_size);
}

folder_instance::folder_instance(exfat& f,const exfat::record* file) : \
	file_instance(f,nullptr,literal(),0,file) {}

folder_instance::~folder_instance(void){
	assert(objlock.is_locked() && objlock.is_exclusive());
	assert(file_table.empty());
}

file_instance* folder_instance::open(const span<char>& str,byte mode){
	lock_guard<rwlock> guard(objlock);
	auto it = file_table.find(str);
	if (it != file_table.end()){
		if ((*it)->is_folder()){
			if (0 == (mode & ALLOW_FOLDER))
				return nullptr;
		}
		else{
			if (0 == (mode & ALLOW_FILE))
				return nullptr;
		}
		(*it)->acquire();
		return *it;
	}
	auto hash_value = exfat::name_hash(str);

	reader rec(*this);
	do{
		auto stat = rec.step(mode | rec.CHECK_HASH,hash_value);
		if (stat != 0){
			break;
		}
		auto file_name = rec.get_name();
		if (!exfat::name_equal(file_name,str))
			continue;
		auto file = rec.get_record();
		assert(file && file->name_hash == hash_value);
		auto index = rec.get_index();
		index = (index > file->entrance_size) ? (index - file->entrance_size - 1) : 0;
		file_instance* instance = nullptr;
		
		if (file->attributes & FOLDER){
			assert(mode & ALLOW_FOLDER);
			if (file->valid_size != file->alloc_size){
				dbgprint("size corrupted in folder %s",file_name.c_str());
				break;
			}
			instance = new folder_instance(clusters.host(),this,move(file_name),index,file);
		}
		else{
			assert(mode & ALLOW_FILE);
			if (file->valid_size > file->alloc_size){
				dbgprint("size corrupted in file %s",file_name.c_str());
				break;
			}
			instance = new file_instance(clusters.host(),this,move(file_name),index,file);
		}
		file_table.insert(instance);
		++ref_count;
		return instance;
			
	}while(true);
	// TODO returns error code
	return nullptr;
}

void folder_instance::close(file_instance* ptr){
	//assert(ptr->objlock.is_locked() && ptr->objlock.is_exclusive());
	assert(ptr->get_parent() == this);
	lock_guard<rwlock> guard(objlock);
	auto it = file_table.find(ptr->get_name());
	assert(it != file_table.end() && *it == ptr);
	file_table.erase(it);
	assert(ref_count);
	if (--ref_count)
		return;
	guard.drop();
	delete this;
}

bool folder_instance::update_size(file_instance* inst){
	assert(inst->is_locked());

	lock_guard<rwlock> guard(objlock);
	reader rec(*this,inst->get_index());

	auto stat = rec.step(ALLOW_FILE | ALLOW_FOLDER);
	if (stat != 0)
		return false;
	auto index = rec.get_index();
	auto file = rec.get_record();
	assert(file);

	if (inst->get_index() + file->entrance_size + 1 != index)
		return false;

	file->valid_size = inst->get_valid_size();
	file->alloc_size = inst->get_size();

	return rec.update(inst->get_index());
}

word folder_instance::reader::checksum(const void* ptr,dword limit){
	word sum = 0;
	for (dword i = 0;i < limit;++i){
		if (i == 2 || i == 3)
			continue;
		sum = (((sum & 1) ? 0x8000 : 0) | (sum >> 1)) + ((const byte*)ptr)[i];
	}
	return sum;
}

byte folder_instance::reader::step(byte mode,word hash){
	byte stat = EOF_FAILURE;
	assert(inst.is_locked());
	//lock_guard<folder_instance> guard(inst,rwlock::SHARED);
	dword limit = inst.get_valid_size() / exfat::record_size;
	const auto cluster_size = inst.clusters.host().cluster_size();
	disk_interface::slot* block = nullptr;
	// safe to read up to the end of sector
	while(index < limit){
		auto offset = index*exfat::record_size;
		auto lba = inst.get_lba(offset);
		if (!lba){
			stat = FS_FAILURE;
			break;
		}
		auto count = dm.count(lba);
		dword block_size = cluster_size - offset % cluster_size;
		offset &= SECTOR_MASK;
		block_size = min<dword>(block_size,count*SECTOR_SIZE - offset);
		assert(block_size);
		count = align_up(block_size,SECTOR_SIZE)/SECTOR_SIZE;
		if (block)
			dm.relax(block);
		block = dm.get(lba,count,false);
		if (!block){
			stat = MEDIA_FAILURE;
			break;
		}
		auto rec = (const record*)block->data(lba) + index % exfat::record_per_sector;
		do{
			auto type = rec->data[0];
			if (0 == type){
				stat = EOF_FAILURE;
				goto done;
			}
			if (mode && type > 0x80){
				switch(type){
					case 0x85:
					{
						if (rec->data[1] < 2)
							break;
						auto file_type = (rec->data[4] & FOLDER) ? ALLOW_FOLDER : ALLOW_FILE;
						if (0 == (file_type & mode))
							break;
						list.clear();
						list.push_back(*rec);
						continue;
					}
					case 0xC0:
						if (list.size() != 1)
							break;
						if (mode & CHECK_HASH){
							auto file_hash = *(const word*)(rec->data + 4);
							if (hash != file_hash)
								break; 
						}
						list.push_back(*rec);
						continue;
					case 0xC1:
					{
						if (list.size() < 2)
							break;
						list.push_back(*rec);
						auto count = list.front().data[1];
						if (list.size() < count)
							continue;
						// checksum
						word sum = checksum(list.data(),exfat::record_size*list.size());
						auto file_sum = *(const word*)(list.front().data + 2);
						if (sum != file_sum)
							break;
						}
						// got file
						++index;
						stat = 0;
						goto done;
				}
			}
			list.clear();
		}while(++rec,++index % exfat::record_per_sector);
	}
done:
	if (stat)
		list.clear();
	if (block)
		dm.relax(block);
	return stat;
}

bool folder_instance::reader::update(dword base,bool name){
	assert(inst.is_locked());
	auto file = get_record();
	if (file == nullptr || (dword)(file->entrance_size + 1) != list.size())
		return false;
	
	dword limit = inst.get_valid_size() / exfat::record_size;
	if (base + file->entrance_size + 1 != index || index > limit)
		return false;
	
	if (name){
		dbgprint("folder_instance::reader::update name not supported");
		return false;
	}
	file->checksum = checksum(file,exfat::record_size*list.size());
	auto offset = base * exfat::record_size;
	auto lba = inst.get_lba(offset);
	if (!lba)
		return false;
	auto block = dm.get(lba,1,false);
	if (!block)
		return false;
	dm.upgrade(block,lba,1);
	auto ptr = (byte*)block->data(lba) + (offset & SECTOR_MASK);
	if ((offset & SECTOR_MASK) <= SECTOR_SIZE - exfat::record_size){
		memcpy(ptr,file,2*exfat::record_size);
		dm.relax(block);
		return true;
	}
	else do{
		memcpy(ptr,file,exfat::record_size);
		dm.relax(block);
		offset += exfat::record_size;
		lba = inst.get_lba(offset);
		if (!lba){
			break;
		}
		block = dm.get(lba,1,false);
		if (!block){
			break;
		}
		dm.upgrade(block,lba,1);
		ptr = (byte*)block->data(lba) + (offset & SECTOR_MASK);
		memcpy(ptr,(const byte*)file + exfat::record_size,exfat::record_size);
		dm.relax(block);
		return true;
	}while(false);
	dbgprint("folder_instance::reader::update failed updating cross-cluster record, record may corrupt");
	return false;
}

exfat::record* folder_instance::reader::get_record(void) const{
	if (list.size() < 2)
		return nullptr;
	return (exfat::record*)(list.data());
}

literal folder_instance::reader::get_name(bool label_folder) const{
	auto file = get_record();
	if (file == nullptr || (dword)(file->entrance_size + 1) != list.size())
		return literal();
	label_folder = (label_folder && (file->attributes & FOLDER));
	char name[align_up(file->name_size + label_folder,0x10)];
	auto name_list = (const name_part*)(file + 1);
	for (unsigned i = 0;i < file->name_size;++i){
		if (2 + i/0xF > file->entrance_size)
			return literal();
		auto ch = name_list[i/0xF].str[i%0xF];
		if (ch == 0 || ch & 0xFF00)
			return literal();
		name[i] = ch & 0xFF;
	}
	if (label_folder)
		name[file->name_size] = '/';
	return literal(name,name + file->name_size + label_folder);
}