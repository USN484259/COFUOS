#pragma once

#ifdef __GNUC__

#include <stdint.h>
#include <stddef.h>

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef uint64_t qword;

#else
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned long long qword;

typedef struct{
	qword low;
	qword high;
}oword;

#pragma warning(disable: 4464 4626 4514 4710 4711 4820 5027 5045 5220)

#endif

//typedef qword size_t;

static_assert(sizeof(byte) == 1, "size_byte_assert_failed");
static_assert(sizeof(word) == 2, "size_word_assert_failed");
static_assert(sizeof(dword) == 4, "size_dword_assert_failed");
static_assert(sizeof(qword) == 8, "size_qword_assert_failed");
static_assert(sizeof(size_t) == 8, "size_sizet_assert_failed");
static_assert(sizeof(void*) == 8, "size_pointer_assert_failed");

namespace UOS{
	
	//typedef void (*fun)(void);
	//typedef void (*exception_handler)(void* addr,void* stk,qword type,qword argu);
	typedef void (*procedure)(void);
	

}
