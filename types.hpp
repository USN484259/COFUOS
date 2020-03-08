#pragma once
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned long long qword;

typedef struct{
	qword low;
	qword high;
}oword;

#pragma warning(disable: 4626 4514 4710 4711 4820 5027)

static_assert(sizeof(byte) == 1, "size_assert_failed");
static_assert(sizeof(word) == 2, "size_assert_failed");
static_assert(sizeof(dword) == 4, "size_assert_failed");
static_assert(sizeof(qword) == 8, "size_assert_failed");
static_assert(sizeof(size_t) == 8, "size_assert_failed");
static_assert(sizeof(void*) == 8, "size_assert_failed");

namespace UOS{
	
	//typedef void (*fun)(void);
	//typedef void (*exception_handler)(void* addr,void* stk,qword type,qword argu);

	
	enum stopcode : qword{
		failed,
		assert_failed,
		not_implemented,
		bad_alloc,
		out_of_range,
		invalid_argument,
		null_deref,
		corrupted
		//inconsistent,
	};
}