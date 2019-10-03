#include "FAT32.hpp"
#include <cstring>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <map>
using namespace std;


FAT32::FAT32_corrupted::FAT32_corrupted(void) : exception("FAT32_corrupted") {}


FAT32::FAT32(MBR& mbr, dword b, dword l) : hdd(mbr), base(b), limit(l), sector_per_cluster(0), table{ 0 }, root(0),end_of_chain(0x0FFFFFFF) {
	byte buffer[0x200];
	hdd.read(buffer, base, 1);

	//TODO check FAT32 system here

	sector_per_cluster = *(buffer + 0x0D);
	table[0] = base + *(word*)(buffer + 0x0E);
	table[1] = table[0] + *(dword*)(buffer + 0x24);

	root = table[1] + *(dword*)(buffer + 0x24);

	hdd.read(buffer, table[0], 1);
	dword eoc = *((dword*)buffer + 1);
	if ((eoc & 0x0FFFFFFF) >= 0x0FFFFFF0)
		end_of_chain = eoc;


	dir_trace.push_back(2);


	dir_get();

}

FAT32::~FAT32(void) {
}


bool FAT32::file_record::valid(void) const {
	for (auto i = 0; i < 11; i++) {	//trick: name+ext 11 bytes
		if (!name[i])
			return false;
	}

	if (!date_time)
		return false;


	return true;
}

dword FAT32::file_record::cluster(void) const {
	return (((dword)cluster_high) << 16) | cluster_low;
}

void FAT32::file_record::cluster(dword c) {
	cluster_low = (word)c;
	cluster_high = (word)(c >> 16);
}

bool FAT32::file_record::operator==(const string& str) const {

	unsigned pos = 8;
	auto it = str.cbegin();
	while (pos && name[--pos] == ' ');

	for (unsigned i = 0; i <= pos;++it, ++i) {
		if (it == str.cend())
			return false;
		if (toupper(*it) != toupper(name[i]))
			return false;
	}
	
	pos = 3;
	while (pos && ext[--pos] == ' ');

	if (it == str.cend())
		return 0 == pos;

	if (*it != '.')
		return false;

	++it;

	for (unsigned i = 0; i <= pos; ++it, ++i) {
		if (it == str.cend())
			return false;
		if (toupper(*it) != toupper(ext[i]))
			return false;
	}

	if (it == str.cend())
		return true;

	return false;

}

bool FAT32::valid_cluster(dword cluster) {
	if (cluster < 2)
		return false;
	if ((cluster & 0x0FFFFFFF) >= 0x0FFFFFF0)
		return false;

	return true;
}


void FAT32::dir_get(void) {
	//stringstream ss(ios::in | ios::out|ios::binary);
	//chain_get(ss, dir_trace.back());
	file_record* buffer = new file_record[0x200 * sector_per_cluster/sizeof(file_record)];
	dir_list.clear();
	dword cluster = dir_trace.back();
	if (!valid_cluster(cluster))
		throw FAT32_corrupted();
	while (valid_cluster(cluster)) {
		cluster = chain_get(buffer, cluster);

		dir_list.insert(dir_list.cend(), buffer, (buffer + 0x200 * sector_per_cluster / sizeof(file_record)));

		
	}
	delete[] buffer;

	//assert(dir_list.begin() != dir_list.end());

	assert(0 == dir_list.size() % (0x200 * sector_per_cluster / sizeof(file_record)));

}

void FAT32::dir_set(void) {
	size_t record_per_cluster = 0x200 * sector_per_cluster / sizeof(file_record);

	size_t size = dir_list.size();

	if (size % record_per_cluster) {
		size += record_per_cluster - size % record_per_cluster;
		dir_list.resize(size);

	}
	assert(0 == dir_list.size() % record_per_cluster);

	auto it = dir_list.cbegin();

	chain_writer writer(*this,dir_trace.back());

	while (it != dir_list.cend()) {
		writer.chain_set(&*it);
		it += record_per_cluster;
	}

	assert(!writer.head());

	//auto buffer = dir_list.data();
	//size = chain_set(dir_trace.back(), stringstream(string((const char*)buffer, (const char*)(buffer + dir_list.size())), ios::out | ios::binary));
	//assert(0 == size % (0x200 * sector_per_cluster));

}

dword FAT32::chain_get(void* dest, dword cluster) {
	if (!valid_cluster(cluster))
		throw FAT32_corrupted();

	hdd.read(dest, root + sector_per_cluster*(cluster - 2), sector_per_cluster);

	byte buffer[0x200];

	hdd.read(buffer, table[0] + cluster / 128, 1);
	return *((dword*)buffer + cluster % 128);

}

FAT32::chain_writer::chain_writer(FAT32& f, dword c) : fs(f), header(0), prev(0), cluster(c) {}

FAT32::chain_writer::~chain_writer(void) {
	//free following blocks
	dword eoc = fs.end_of_chain;
	while (prev && fs.valid_cluster(cluster)) {
		*(buffer + prev % 128) = eoc;
		record.insert(pair<dword, dword>(prev, eoc));

		if (prev != cluster) {

			fs.hdd.write(fs.table[0] + prev / 128, buffer, 1);


			fs.hdd.read(buffer, fs.table[0] + cluster / 128, 1);
		}

		prev = cluster;
		cluster = *(buffer + prev % 128);

		eoc = 0;

	}

	if (prev)
		fs.hdd.write(fs.table[0] + prev / 128, buffer, 1);

	//apply record to table[1]

	prev = 0xFFFFFFFF;
	for (auto it = record.cbegin(); it != record.cend(); ++it) {
		dword cur = fs.table[1] + it->first / 128;
		if (prev != cur) {
			if (prev != 0xFFFFFFFF)
				fs.hdd.write(prev, buffer, 1);

			fs.hdd.read(buffer, cur, 1);

			prev = cur;

		}

		*(buffer + it->first % 128) = it->second;

	}

	if (prev != 0xFFFFFFFF)
		fs.hdd.write(prev, buffer, 1);

}

dword FAT32::chain_writer::head(void) const {
	return header;
}


void FAT32::chain_writer::chain_set(const void* source) {
	if (!fs.valid_cluster(cluster)) {
		cluster = fs.allocate(record);
		//assumes that buffer hold prev's sector
		//if no buffer
		if (!prev) {
			header = cluster;
		}
		else {
			//if new cluster conflicts read again
			if (prev / 128 == cluster / 128) {
				fs.hdd.read(buffer, fs.table[0] + prev / 128, 1);
				//buffer[cluster % 128] = fs.end_of_chain;
			}

			*(buffer + prev % 128) = cluster;

			record.insert(pair<dword, dword>(prev, cluster));

			fs.hdd.write(fs.table[0] + prev / 128, buffer, 1);
		}

	}
	fs.hdd.write(fs.root + fs.sector_per_cluster*(cluster - 2), source, fs.sector_per_cluster);

	//reduce read times
	if (!prev || prev / 128 != cluster / 128)
		fs.hdd.read(buffer, fs.table[0] + cluster / 128, 1);


	prev = cluster;

	cluster = *(buffer + cluster % 128);
}


FAT32::file_list::file_list(const vector<FAT32::file_record>& d) : dir(d), it(d.cend()),valid(true) {}

bool FAT32::file_list::step(void) {

	if (valid) {

		if (it == dir.cend())
			it = dir.cbegin();
		else
			++it;

		while (it != dir.cend() && !it->valid()) {
			++it;
		}

		if (it == dir.cend())
			valid = false;
	}

	return valid;

}

string FAT32::file_list::name(void) const {
	if (!valid)
		throw out_of_range("file_list iterator out of range");
	string res = string(it->name, 8);
	return res.substr(0, res.find_last_not_of(' ') + 1);
}

string FAT32::file_list::extension(void) const {
	if (!valid)
		throw out_of_range("file_list iterator out of range");
	string res = string(it->ext, 3);
	return res.substr(0, res.find_last_not_of(' ') + 1);
}

string FAT32::file_list::fullname(void) const {
	if (!valid)
		throw out_of_range("file_list iterator out of range");
	string res = name();
	string ext = extension();

	if (!ext.empty()) {
		res += '.';
		res += ext;
	}
	return res;
}

size_t FAT32::file_list::size(void) const {
	if (!valid)
		throw out_of_range("file_list iterator out of range");
	return it->size;
}

bool FAT32::file_list::is_folder(void) const {
	if (!valid)
		throw out_of_range("file_list iterator out of range");
	return (it->attrib & 0x10) ? true : false;
}


FAT32::file_list FAT32::list(void) const {
	return file_list(dir_list);
}

/*

size_t FAT32::chain_set(dword cluster, istream& in) {
	assert(cluster >= 2);
	byte* buffer = new byte[0x200 * sector_per_cluster];
	byte cluster_buf[0x200] = { 0 };
	map<dword, dword> record;
	dword prev = 0;

	size_t length = 0;

	while(true){
		size_t cur_len = in.readsome((char*)buffer, 0x200 * sector_per_cluster);
		if (!cur_len)
			break;


		length += cur_len;

		if ((cluster & 0x0FFFFFFF) >= 0x0FFFFFF0) {
			assert(prev);
			cluster = allocate(record);
			*((dword*)cluster_buf + prev % 128) = cluster;

			hdd.write(table[0] + prev / 128, cluster_buf, 1);

			record.insert(pair<dword, dword>(prev, cluster));
		}

		hdd.write(root + sector_per_cluster*(cluster - 2), buffer, sector_per_cluster);

		prev = cluster;


		hdd.read(cluster_buf, table[0] + prev / 128, 1);

		cluster = *((dword*)cluster_buf + prev % 128);

		if (cur_len != 0x200 * sector_per_cluster)
			break;

	}

	delete[] buffer;
	while (prev && (cluster & 0x0FFFFFFF) < 0x0FFFFFF0) {
		*((dword*)cluster_buf + prev % 128) = end_of_chain;
		hdd.write(table[0] + prev / 128, cluster_buf, 1);

		record.insert(pair<dword, dword>(prev, end_of_chain));

		prev = cluster;

		hdd.read(cluster_buf, table[0] + prev / 128, 1);

		cluster = *((dword*)cluster_buf + prev % 128);

	}

	prev = 0;
	for (auto it = record.cbegin(); it != record.cend(); ++it) {
		dword cur = table[1] + it->first / 128;
		if (prev != cur) {
			if (prev) {
				hdd.write(prev, cluster_buf, 1);
			}

			hdd.read(cluster_buf, cur, 1);
			prev = cur;
		}

		*((dword*)cluster_buf + it->first % 128) = it->second;


	}

	if (prev)
		hdd.write(prev, cluster_buf, 1);

	return length;

}
*/

dword FAT32::allocate(map<dword, dword>& record) {
	dword buffer[0x200/sizeof(dword)];
	for (dword sector = table[0]; sector != root; ++sector) {
		hdd.read(buffer, sector, 1);

		for (unsigned i = 0; i < 0x200 / sizeof(dword); ++i) {
			if (!buffer[i]) {

				dword cluster = (sector - table[0]) * 128 + i;
				record.insert(pair<dword, dword>(cluster, end_of_chain));

				buffer[i] = end_of_chain;

				hdd.write(sector, buffer, 1);

				return cluster;
			}
		}

	}

	throw bad_alloc();
}


bool FAT32::put(const string& name, istream& source) {
	auto it = find(dir_list.begin(), dir_list.end(), name);


	if (it == dir_list.end()) {
		//TODO: Create file not implemented
		return false;
	}


	if (it->attrib & 0x10)	//folders
		return false;

	chain_writer writer(*this,it->cluster());
	byte* buffer = new byte[0x200 * sector_per_cluster];
	dword new_size = 0;

	while (true) {
		source.read((char*)buffer, 0x200 * sector_per_cluster);

		dword len = (dword)source.gcount();

		if (!len)
			break;

		if (len < (unsigned)0x200 * sector_per_cluster)
			memset(buffer + len, 0, 0x200 * sector_per_cluster - len);

		writer.chain_set(buffer);
		new_size += len;
	}

	delete[] buffer;

	if (writer.head()) {
		assert(it->size != new_size);
		it->cluster(writer.head());
	}

	if (it->size != new_size) {
		it->size = new_size;

		dir_set();

	}
	return true;

}

bool FAT32::get(ostream& out, const string& name) {
	auto it = find(dir_list.begin(), dir_list.end(), name);

	if (it == dir_list.end())
		return false;

	dword length = it->size;
	dword cluster = it->cluster();

	byte* buffer = new byte[0x200 * sector_per_cluster];

	while (valid_cluster(cluster)) {
		cluster = chain_get(buffer, cluster);

		if (length <= (unsigned)0x200 * sector_per_cluster) {
			out.write((const char*)buffer, length);
			break;
		}

		out.write((const char*)buffer, 0x200 * sector_per_cluster);
		length -= 0x200 * sector_per_cluster;

	}

	delete[] buffer;
	return true;
}


/*


using namespace UOS;

FAT32::FATable::FATable(HDD& d, qword base) : hdd(d), lba_base(base) {}


dword& FAT32::FATable::operator[](size_t index) {
	auto it = record.find(index / 0x80);

	if (it == record.end()) {
		dword buf[0x80];
		if (!hdd.read(buf, lba_base + index / 0x80, 1))
			throw runtime_error("disk IO failure");
		it = record.emplace((const dword)(index / 0x80), buf).first;
	}

	return it->second[index % 0x80];
}




FAT32::FAT32(HDD& h, qword b, qword l) : FileSystem(h, b, l) {

}

*/