#include "fs_util.h"

using namespace std;

filepath::filepath(const string& s) : str(s), path_offset(s.find_last_of('\\')), ext_offset(s.find_last_of('.')) {}

filepath::filepath(const char* s) : str(s), path_offset(str.find_last_of('\\')), ext_offset(str.find_last_of('.')) {}

string filepath::path(void) const {
	if (path_offset >= str.size())
		return string();
	return str.substr(0, path_offset);
}

string filepath::name_ext(void) const {
	if (path_offset + 1 >= str.size())
		return str;
	return str.substr(path_offset + 1);
}

string filepath::name(void) const {
	if (path_offset >= ext_offset)
		return str.substr(0, ext_offset);
	return str.substr(path_offset + 1, ext_offset);
}

string filepath::extention(void) const {
	if (ext_offset + 1 >= str.size())
		return string();
	return str.substr(ext_offset + 1);
}

filepath::operator const std::string& (void) const {
	return str;
}

size_t file_finder::find(const char* str) {
	WIN32_FIND_DATA info;
	HANDLE h = FindFirstFile(str, &info);
	if (h == INVALID_HANDLE_VALUE)
		return 0;
	size_t cnt = 0;
	do {
		if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		filelist.emplace(info.cFileName);
		cnt++;

	} while (FindNextFile(h, &info));
	FindClose(h);
	return cnt;
}

size_t file_finder::find(const string& str) {
	return find(str.c_str());
}

unordered_set<string>::const_iterator file_finder::begin(void) const {
	return filelist.cbegin();
}

unordered_set<string>::const_iterator file_finder::end(void) const {
	return filelist.cend();
}

size_t file_finder::size(void) const {
	return filelist.size();
}

void file_finder::clear(void) {
	filelist.clear();
}