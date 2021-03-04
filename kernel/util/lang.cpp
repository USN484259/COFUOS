#include "lang.hpp"
#include "util.hpp"
#include "intrinsics.hpp"
#include "memory/include/heap.hpp"
#include "memory/include/vm.hpp"
#include "assert.hpp"
#include "fasthash.h"

using namespace UOS;

constexpr size_t huge_size = 0x10000;


void* operator new(size_t len){
	if (!len)
		bugcheck("new invalid size %x",len);
	if (len < huge_size){
		auto res = heap.allocate(len);
		if (!res)
			bugcheck("new bad alloc size %x",len);
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
		bugcheck("new bad alloc size %x",len);
	}

}

void* operator new(size_t,void* pos){
	return pos;
}


void operator delete(void* p,size_t len){
	if (!p)
		return;
	assert(len);
	if (len < huge_size){
		heap.release(p,len);
	}
	else{
		assert(0 == ((qword)p & PAGE_MASK));
		auto page_count = align_up(len,PAGE_SIZE) >> 12;
		vm.release((qword)p,page_count);
	}
}

void* operator new[](size_t len){
	qword* p=(qword*)operator new(len+sizeof(qword));
	*p=(qword)len;	//place size in the first qword of block
	return p+1;	//return the rest of block
}

void operator delete[](void* p){
	if (!p)
		return;
	qword* b=(qword*)p;
	--b;	//point back to the block head
	operator delete(b,*b);	//block size at head
}

extern "C" {
    //__chkstk in hal.asm

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