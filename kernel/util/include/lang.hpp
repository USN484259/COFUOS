#pragma once
#include "types.hpp"

extern "C"{
	void* memset(void*,int,size_t);
	void* memcpy(void*,const void*,size_t);
}
namespace UOS{
    void zeromemory(void*,size_t);

    typedef qword* va_list;
}

#define va_start(l,a) (l = (qword*)(&a) + 1)
#define va_end(l) (l = nullptr)
#define va_arg(l,T) ((T)(*l++))