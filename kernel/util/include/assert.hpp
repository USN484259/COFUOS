#pragma once
#include "types.h"

#ifndef NDEBUG
#include "lang.hpp"

#define assert(e) ( (e) ? 0:(bugcheck("assert_failed: \'%s\' @ %s:%d",#e,__FILE__,__LINE__),-1) )
#define IF_assert assert(0 == (read_eflags() & 0x0200))

#else

#define assert(e)
#define IF_assert

#endif



