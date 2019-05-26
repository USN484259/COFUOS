#pragma once
#include "types.hpp"



#ifdef _DEBUG
#define assert(c) ( (c)?0:dbgbreak() )


int dbgbreak(void);
#else
#define assert(c) (c)


#endif



extern "C" qword IF_get(void);



#define IF_assert(c) assert(IF_get()?c:!c)
