#include "file.hpp"

using namespace UOS;

string basic_file::file_name(void) const{
	auto it = name.find_last_of('/');
	if (it == name.end()){
		bugcheck("invalid file name %s",name.c_str());
	}
	return name.substr(++it,name.end());
}

string basic_file::path_name(void) const{
	auto it = name.find_last_of('/');
	if (it == name.end()){
		bugcheck("invalid file name %s",name.c_str());
	}
	return name.substr(name.begin(),++it);
}

extern "C" dword test_file_size;
extern "C" byte test_file_base;

file_stub::file_stub(void) : basic_file("/test.exe") {}

basic_file::TYPE file_stub::type(void) const{
	return FILE;
}

qword file_stub::attribute(void) const{
	return READONLY;
}

size_t file_stub::size(void) const{
	return test_file_size;
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

size_t file_stub::read(void* dst,size_t length){
	if (offset >= test_file_size)
		return false;
	auto sor = (&test_file_base) + offset;
	length = min(length,test_file_size - offset);
	memcpy(dst,sor,length);
	offset += length;
	return length;
}

size_t file_stub::write(const void*,size_t){
	return 0;
}