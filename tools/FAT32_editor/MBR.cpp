#include "MBR.hpp"

using namespace std;

inline qword bitswap64(qword data){
	__asm__ (
		"bswap %0"
		: "+r" (data)
	);
	return data;
}

MBR::MBR(const std::string& filename) : vhd(filename, ios::in | ios::out | ios::binary) {
	if (!vhd.is_open())
		throw runtime_error(string("cannot open vhd file\t")+filename);
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

	vhd.seekg(0, ios::beg);
	vhd.read((char*)buffer, 0x200);
	byte* pt = buffer + 0x1BE;
	for (unsigned i = 0; i < 4;pt += 0x10, i++) {
		if (*pt == 0x80)
			boot_index = i;
		if (*(pt + 4) == 0x0C)
			partitions[i] = new FAT32(*this, *(dword*)(pt + 8), *(dword*)(pt + 0x0C));

	}

}

MBR::~MBR(void) {
	for (unsigned i = 0; i < 4; i++) {
		delete partitions[i];
	}
}

void MBR::read(void* buffer, qword lba, size_t count) {
	if (lba + count > lba_limit)
		throw out_of_range("LBA overflow");
	vhd.seekg(lba * 0x200);
	vhd.read((char*)buffer, 0x200 * count);
}

void MBR::write(qword lba, const void* buffer, size_t count) {
	if (lba + count > lba_limit)
		throw out_of_range("LBA overflow");
	vhd.seekp(lba * 0x200);
	vhd.write((const char*)buffer, 0x200 * count);
}

FAT32* MBR::partition(unsigned index) {
	if (index >= 4)
		throw runtime_error("partition overflow");
	return partitions[index];
}

qword MBR::size(void) const {
	return lba_limit;
}