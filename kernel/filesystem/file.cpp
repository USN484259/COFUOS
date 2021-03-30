#include "file.hpp"

using namespace UOS;

literal basic_file::file_name(void) const{
	auto it = find_last_of(name.begin(),name.end(),'/');
	if (it == name.end()){
		bugcheck("invalid file name %s",name.c_str());
	}
	return literal(++it,name.end());
}

literal basic_file::path_name(void) const{
	auto it = find_last_of(name.begin(),name.end(),'/');
	if (it == name.end()){
		bugcheck("invalid file name %s",name.c_str());
	}
	return literal(name.begin(),++it);
}

struct FILE_INFO{
	void* base;
	dword size;
	char name[0x14];
}__attribute__((packed));

extern "C" FILE_INFO file_list;

file_stub* file_stub::open(literal&& filename){
	auto list = &file_list;
	while(list->base){
		if (filename == (const char*)list->name){
			return new file_stub(move(filename),list->base,list->size);
		}
		++list;
	}
	return nullptr;
}

file_stub::file_stub(literal&& filename,void* ptr,dword len) : \
	basic_file(move(filename)), base((byte*)ptr), length(len) {}

basic_file::FILETYPE file_stub::file_type(void) const{
	return FILE;
}

qword file_stub::attribute(void) const{
	return READONLY;
}

size_t file_stub::size(void) const{
	return length;
}

bool file_stub::seek(size_t off){
	if (off > size())
		return false;
	offset = off;
	return true;
}

size_t file_stub::tell(void) const{
	return offset;
}

dword file_stub::read(void* dst,dword len){
	if (offset >= length)
		return false;
	auto sor = base + offset;
	len = min<dword>(len,length - offset);
	memcpy(dst,sor,len);
	offset += len;
	return len;
}

dword file_stub::write(const void*,dword){
	return 0;
}