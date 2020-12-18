#pragma once
#include "types.hpp"

#ifdef _DEBUG
#include "bugcheck.hpp"

#define assert(e) ( (e) ? 0:(BugCheck(assert_failed,#e),-1) )
#define IF_assert assert(0 == (__readeflags() & 0x0200))

#else

#define assert(e) __assume(e)
#define IF_assert

#endif



