#include <iostream>
#include <deque>
#include <random>
#include <algorithm>
#include <iomanip>
#include <string>

#include "page_mem.hpp"
#include "..\..\heap.hpp"

using namespace UOS;
using namespace std;

void _check(bool val) {
	if (!val)
		abort();
}

int main(void) {
	size_t block_size = 0x200 * 0x1000;
	void* block = page_mem::allocate(block_size / 0x1000);
	string str;
	do{
		heap pool;
		pool.expand(block, block_size);

		deque<std::pair<void*,size_t>> blocks;
		random_device rng;

		size_t total_size = 0;
		while(pool.capability()) {
			size_t len = 1 + (rng() & 0x0FFF);
			void* ptr = pool.allocate(len);
			if (ptr) {
				blocks.push_back(std::pair<void*, size_t>(ptr, len));
				total_size += len;
			}
		}

		cout << dec << blocks.size() << " times allocation\t" << hex << total_size << " in " << hex << block_size << "\tEfficiency " << fixed << setprecision(1) << (double)total_size / block_size * 100 << '%' << endl;

		shuffle(blocks.begin(), blocks.end(),rng);

		for (auto it = blocks.cbegin(); it != blocks.cend(); ++it) {
			pool.release(it->first, it->second);
		}

	} while (getline(cin,str),!cin.fail());

	page_mem::release(block);
	return 0;
}