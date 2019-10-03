#pragma once
#include "types.h"
#include "MBR.hpp"
//#include "fs.hpp"
#include <exception>
#include <map>
#include <vector>
//#include <deque>
#include <string>

class MBR;

class FAT32 {

#pragma pack(push)
#pragma pack(1)
	struct file_record {
		char name[8];
		char ext[3];
		byte attrib;
		qword reserved;
		word cluster_high;
		dword date_time;
		word cluster_low;
		dword size;

		bool valid(void) const;
		dword cluster(void) const;
		void cluster(dword);
		bool operator==(const std::string&) const;
	};
#pragma pack(pop)

	class chain_writer {

		friend class FAT32;

		FAT32& fs;
		dword header;
		dword prev;
		dword cluster;

		std::map<dword, dword> record;

		dword buffer[0x200 / sizeof(dword)];

	public:
		chain_writer(FAT32&, dword = 0x0FFFFFF0);
		chain_writer(const chain_writer&) = delete;
		~chain_writer(void);
		void chain_set(const void* source);

		dword head(void) const;
	};



public:
	class FAT32_corrupted : public std::exception {
	public:
		FAT32_corrupted(void);
	};

	class file_list {

		friend class FAT32;

		const std::vector<file_record>& dir;
		std::vector<file_record>::const_iterator it;
		bool valid;
		file_list(const std::vector<file_record>&);

	public:

		bool step(void);
		std::string name(void) const;
		std::string extension(void) const;
		std::string fullname(void) const;
		size_t size(void) const;
		bool is_folder(void) const;


	};

private:

	MBR& hdd;
	dword base;
	dword limit;
	dword table[2];
	dword root;
	dword end_of_chain;
	byte sector_per_cluster;

	std::vector<dword> dir_trace;

	std::vector<file_record> dir_list;

private:
	static bool valid_cluster(dword);
	void dir_get(void);
	void dir_set(void);
	dword chain_get(void* dest, dword cluster);

	dword allocate(std::map<dword, dword>& record);

public:
	FAT32(MBR& mbr, dword base, dword length);
	~FAT32(void);
	//std::string directory(void) const;
	//void directory(const std::string&);

	//size_t list(std::deque<std::string>&);

	file_list list(void) const;

	bool put(const std::string& name, std::istream& source);

	bool get(std::ostream& dest, const std::string& name);


};
