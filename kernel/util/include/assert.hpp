#pragma once
#include "types.hpp"

#ifndef NDEBUG
#include "bugcheck.hpp"

#define assert(e) ( (e) ? 0:(BugCheck(assert_failed,#e,__FILE__,__LINE__),-1) )
#define IF_assert assert(0 == (read_eflags() & 0x0200))

#else

#define assert(e)
#define IF_assert

#endif



