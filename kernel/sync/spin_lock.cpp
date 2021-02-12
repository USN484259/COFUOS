#include "spin_lock.hpp"
#include "intrinsics.hpp"
#include "assert.hpp"

using namespace UOS;

spin_lock::spin_lock(void) : state(0) {}

bool spin_lock::try_lock(void) {
    return cmpxchg<dword>(&state,1,0) ? false : true;
}

void spin_lock::lock(void) {
	size_t cnt = 0;
	while(! try_lock() ){
		if (cnt++ > spin_timeout)
			bugcheck("spin_timeout %x",cnt);
		mm_pause();
	}
	
	assert(1 == state);

	
}

void spin_lock::unlock(void) {
	assert(1 == state);
	
	dword tmp = xchg<dword>(&state,0);
	assert(1 == tmp);
	
}

bool spin_lock::is_locked(void) const {
	return state ? true : false;
	
}