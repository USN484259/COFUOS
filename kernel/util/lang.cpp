#include "lang.hpp"
#include "util.hpp"
#include "intrinsics.hpp"
#include "assert.hpp"
#include "memory/include/heap.hpp"
#include "memory/include/vm.hpp"
#include "fasthash.h"

using namespace UOS;

constexpr size_t huge_size = PAGE_SIZE/2;


void* operator new(size_t len){
	if (!len)
		bugcheck("operator new invalid size %x",len);
	if (len <= huge_size){
		auto res = heap.allocate(len);
		if (!res)
			bugcheck("operator new bad alloc size %x",len);
		return res;
	}
	else{
		//Allocating huge memory block, use reserve & commit
		auto page_count = align_up(len,PAGE_SIZE) >> 12;
		auto vbase = vm.reserve(0,page_count);
		if (vbase){
			auto res = vm.commit(vbase,page_count);
			if (res)
				return (void*)vbase;
			vm.release(vbase,page_count);
		}
		bugcheck("operator new bad alloc size %x",len);
	}

}

void* operator new(size_t,void* pos){
	return pos;
}


void operator delete(void* p,size_t len){
	if (!p)
		return;
	assert(len);
	if (len <= huge_size){
		heap.release(p,len);
	}
	else{
		assert(0 == ((qword)p & PAGE_MASK));
		auto page_count = align_up(len,PAGE_SIZE) >> 12;
		vm.release((qword)p,page_count);
	}
}

extern "C" {

    int atexit(void (*)(void)){
        return 0;
    }

    int 
#ifdef __GNUC__
	__cxa_pure_virtual
#else
	_purecall
#endif
	(void){
        bugcheck("pure virtual call");
    }

}

static qword seed = 0;

qword UOS::rand(void){
    qword tsc = rdtsc();
    seed = fasthash64(&tsc,8,seed);
    return seed;
}

void UOS::srand(qword sd){
    seed = sd;
}