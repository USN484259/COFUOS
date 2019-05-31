#pragma once
#include "types.hpp"
#include "hal.hpp"


#ifdef _DEBUG
#define assert(c) ( (c)?0:dbgbreak() )


#else
#define assert(c) (c)


#endif




#define IF_assert(c) assert(IF_get()?c:!c)
#define page_assert(p) assert(0==((qword)p & 0xFFF))