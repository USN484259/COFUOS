#pragma once
#include "types.hpp"
#include "hal.hpp"

#ifdef _DEBUG


#define assert(a,b) ( ((a)==(b))?0:BugCheck(bad_assert,(qword)a,(qword)b) )
#define assertinv(a,b) ( ((a)!=(b))?0:BugCheck(bad_assert,(qword)a,(qword)b) )
#define assertless(a,b) ( ((a)<(b))?0:BugCheck(bad_assert,(qword)a,(qword)b) )

#else
	

#define assert(a,b)
#define assertinv(a,b)
#define assertless(a,b)

#endif




#define IF_assert(c) assert(c,IF_get())
#define page_assert(p) assert(0,(qword)p & 0xFFF)