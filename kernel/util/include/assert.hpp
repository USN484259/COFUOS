#pragma once
#include "types.hpp"
//#include "hal.hpp"

//#ifndef _TEST


#ifdef _DEBUG
#include "../../cpu/include/hal.hpp"

// #define assert(a,b) ( ((a)==(b))?0:BugCheck(bad_assert,(a),(b) ) )
// #define assertinv(a,b) ( ((a)!=(b))?0:BugCheck(bad_assert,(a),(b) ) )
// #define assertless(a,b) ( ((a)<(b))?0:BugCheck(bad_assert,(a),(b) ) )

#define assert(e) ( (e) ? 0:(BugCheck(assert_failed,#e),-1) )

#else

#define assert(e) __assume(e)

// #define assert(a,b) __assume((a)==(b))
// #define assertinv(a,b) __assume((a)!=(b))
// #define assertless(a,b) __assume ((a)<(b))

#endif

//#else

//extern void _check(bool);

//#define assert(a,b) _check((a)==(b))
//#define assertinv(a,b) _check((a)!=(b))
//#define assertless(a,b) _check ((a)<(b))


//#endif


//#define IF_assert(c) assert(c,IF_get())
//#define page_assert(p) assert(0,(qword)p & 0x0FFF)
//#define MP_assert(s) assert((s)?1:0,mp?(mp->lock_state()?1:0):((s)?1:0))
//#define IF_assert(c) assert(c==IF_get())
#define page_assert(p) assert(0==(qword)p & 0x0FFF)
//#define MP_assert(s) assert( ((s)?1:0) == (mp?(mp->lock_state()?1:0):((s)?1:0)) )
