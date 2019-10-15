#include "page_mem.hpp"
#include <Windows.h>

namespace page_mem {

	void* allocate(size_t pagecount) {
		return VirtualAlloc(NULL, 0x1000 * pagecount, MEM_COMMIT, PAGE_READWRITE);
	}

	void release(void* ptr) {
		VirtualFree(ptr, 0, MEM_RELEASE);
	}

}