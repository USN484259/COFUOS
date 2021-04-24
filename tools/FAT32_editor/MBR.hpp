#pragma once
#include "types.h"
#include "FAT32.hpp"
#include <fstream>
#include <string>

class FAT32;

class MBR {
	std::fstream vhd;
	qword lba_limit = 0;
	FAT32* partitions[4] = {0};
	unsigned boot_index = 0;

public:
	MBR(const std::string& filename);
	~MBR(void);

	unsigned boot_partition(void) const{
		return boot_index;
	}
	FAT32* partition(unsigned index);

	qword size(void) const;
	void read(void* buffer, qword lba, size_t count);
	void write(qword lba, const void* buffer, size_t count);
};
