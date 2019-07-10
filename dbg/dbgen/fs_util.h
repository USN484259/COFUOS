#pragma once
#include <string>
#include <unordered_set>
#include <windows.h>


class filepath {
	const std::string str;
	const size_t path_offset;
	const size_t ext_offset;
public:
	filepath(const std::string&);
	filepath(const char*);
	std::string path(void) const;
	std::string name(void) const;
	std::string name_ext(void) const;
	std::string extention(void) const;
	operator const std::string&(void) const;
};

class file_finder {
	std::unordered_set<std::string> filelist;

public:
	size_t find(const char*);
	size_t find(const std::string&);
	size_t size(void) const;
	void clear();
	std::unordered_set<std::string>::const_iterator begin(void) const;
	std::unordered_set<std::string>::const_iterator end(void) const;

};