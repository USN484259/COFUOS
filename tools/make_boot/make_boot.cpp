#include "types.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>

using namespace std;

inline qword bitswap64(qword data){
	__asm__ (
		"bswap %0"
		: "+r" (data)
	);
	return data;
}

class VHD{
	fstream vhd;
	qword lba_limit = 0;
public:
	VHD(const char* filename) : vhd(filename,ios::in | ios::out | ios::binary){
		if (!vhd.is_open())
			throw runtime_error(string("cannot open file ").append(filename));
		vhd.exceptions(ios::badbit | ios::failbit | ios::eofbit);
		byte buffer[0x200];
		vhd.seekg(-0x200, ios::end);
		qword file_size = vhd.tellg();
		vhd.read((char*)buffer, 0x200);
		lba_limit = bitswap64(*(qword*)(buffer + 0x30)) / 0x200;
		if (lba_limit * 0x200 != file_size)
			throw runtime_error("VHD size mismatch");
		if (*(qword*)(buffer + 0x10) != 0xFFFFFFFFFFFFFFFF)
			throw runtime_error("not fixed VHD");
	}
	~VHD(void) = default;

	void read(qword lba,void* buffer){
		if (lba >= lba_limit)
			throw out_of_range("LBA overflow");
		vhd.seekg(lba * 0x200);
		vhd.read((char*)buffer,0x200);
	}

	void write(qword lba,const void* data){
		if (lba >= lba_limit)
			throw out_of_range("LBA overflow");
		vhd.seekp(lba * 0x200);
		vhd.write((const char*)data,0x200);
	}
};

bool check_exfat(const void* buffer){
	struct exfat_header{
		byte jmp_boot[3];
		char fs_name[8];
		byte zero[0x35];
		qword base;
		qword limit;
		dword fat_offset;
		dword fat_length;
		dword cluster_offset;
		dword cluster_count;
		dword root_dir;
		dword serial_number;
		word fs_version;
		word flags;
		byte sector_shift;
		byte cluster_shift;
		byte fat_count;
		byte drive_select;
		byte use_percent;
		byte reserved[7];
		byte boot_code[0x186];
		word signature;
	};
	static_assert(sizeof(exfat_header) == 0x200,"exfat header size mismatch");
	auto header = (const exfat_header*)buffer;
	if (header->signature != 0xAA55)
		return false;
	if (memcmp(header->fs_name,"EXFAT   ",8))
		return false;
	for (auto i = 0;i < 0x35;++i){
		if (header->zero[i])
			return false;
	}
	if (header->fs_version != 0x0100)
		return false;
	if (header->sector_shift != 9)
		return false;
	
	return true;	
}

int main(int argc,char** argv){
	if (argc < 3) {
		cerr << "Usage: " << argv[0] << " (vhd_file) (boot_code) [loader_code]" << endl;
		return 1;
	}
	try{
		VHD disk(argv[1]);
		qword base = 0;
		byte buffer[0x200*0x0C];
		do{
			disk.read(base,buffer);
			if (check_exfat(buffer))
				break;
			if (!base && buffer[0x1FE] == 0x55 && buffer[0x1FF] == 0xAA){
				// parse as MBR
				for (auto ptr = buffer + 0x1BE;ptr < buffer + 0x1FE;ptr += 0x10){
					switch(ptr[0]){
						case 0x80:
							if (ptr[4] == 0x07){
								base = *(const dword*)(ptr + 0x08);
								break;
							}
						case 0:
							continue;
					}
					break;
				}
				if (base)
					continue;
			}
			throw runtime_error("failed to locate exFAT partition");
		}while(true);
		unsigned i;
		for (i = 1;i < 0x0C;++i){
			disk.read(base + i,buffer + 0x200*i);
		}
		ifstream boot(argv[2],ios::in | ios::binary);
		if (!boot.is_open())
			throw runtime_error(string("cannot open ").append(argv[2]));
		boot.read((char*)(buffer + 0x78),0x188);
		auto len = boot.gcount();
		if (len == 0)
			throw runtime_error("failed to read boot code");
		if (len > 0x186)
			throw runtime_error("boot code too large");
		if (argc > 3){
			ifstream ldr(argv[3],ios::in | ios::binary);
			if (!ldr.is_open())
				throw runtime_error(string("cannot open ").append(argv[3]));
			i = 1;
			while(true){
				ldr.read((char*)buffer + 0x200*i,0x1FC);
				*(dword*)(buffer + 0x200*i + 0x1FC) = 0xAA550000;
				auto len = ldr.gcount();
				if (len != 0x1FC){
					if (ldr.eof()){
						memset(buffer + 0x200*i + len,0,0x1FC - len);
						break;
					}
					else
						throw runtime_error("failed to read boot code");
				}
				if (++i >= 9)
					throw runtime_error("loader code too large");
			}
		}
		// calcute checksum here
		dword sum = 0;
		for (i = 0;i < 0x200*0x0B;++i){
			switch(i){
				case 0x6A:
				case 0x6B:
				case 0x70:
					continue;
			}
			sum = (((sum & 1) ? 0x80000000 : 0) | (sum >> 1)) + buffer[i];
		}
		auto ptr = (dword*)(buffer + 0x200*0x0B);
		for (i = 0;i < 0x80;++i)
			ptr[i] = sum;
		
		//write back
		for (i = 0;i < 0x0C;++i){
			disk.write(base + i,buffer + 0x200*i);
		}
	}
	catch(exception& e){
		cerr << e.what() << endl;
		return 2;
	}
	return 0;
}