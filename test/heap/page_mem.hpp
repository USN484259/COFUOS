#pragma once

namespace page_mem {
	void* allocate(size_t pagecount);
	void release(void* ptr);

}