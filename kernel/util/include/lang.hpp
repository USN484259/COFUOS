#pragma once
#include "types.h"
#include "stdarg.h"


extern "C"{
	void* memset(void*,int,size_t);
	void* zeromemory(void*,size_t);
	void* memcpy(void*,const void*,size_t);
	[[ noreturn ]]
	void bugcheck_raise(void);
	//defined in kdb.cpp
	void dbgprint(const char*,...);
}

void* operator new(size_t);
void* operator new(size_t,void*);
void* operator new[](size_t);

void operator delete(void*,size_t);
void operator delete[](void*);

namespace UOS{
	qword rand(void);
	void srand(qword);
}

#define bugcheck(...) dbgprint(__VA_ARGS__), bugcheck_raise()

#ifndef va_arg
typedef qword* va_list;
#define va_start(l,a) (l = (qword*)(&a) + 1)
#define va_end(l) (l = nullptr)
#define va_arg(l,T) ((T)(*l++))
#endif