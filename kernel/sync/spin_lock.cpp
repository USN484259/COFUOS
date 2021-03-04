#include "spin_lock.hpp"
#include "intrinsics.hpp"
#include "assert.hpp"

using namespace UOS;

#ifdef NDEBUG
#define s_limit (x_value/2)
#else
#define s_limit (0x40)
#endif

spin_lock::spin_lock(void) : state(0) {}

bool spin_lock::try_lock(MODE mode) {
	switch(mode){
		case EXCLUSIVE:
			return (0 == cmpxchg<dword>(&state,x_value,0));
		case SHARED:
		{
			auto cur = state;
			if (cur >= x_value){
				assert(cur == x_value);
				return false;
			}
			if (cur >= s_limit)
				bugcheck("shared lock overflow @ %p : %d locks",this,cur);
			return (cur == cmpxchg(&state,cur + 1,cur));
		}
		default:
			assert(false);
	}
}

void spin_lock::lock(MODE mode) {
	size_t cnt = 0;
	while(! try_lock(mode) ){
		if (cnt++ > spin_timeout)
			bugcheck("spin_timeout %x",cnt);
		mm_pause();
	}
	assert(mode == SHARED ? (state && state < s_limit) :(x_value == state));
}

void spin_lock::unlock(void) {
	auto cur = state;
	assert(cur);
	if (cur >= x_value){
		assert(cur == x_value);
		dword tmp = xchg<dword>(&state,0);
		assert(x_value == tmp);
		return;
	}
	size_t cnt = 0;
	do{
		if (cur == cmpxchg(&state,cur - 1,cur))
			return;
		mm_pause();
		cur = state;
	}while(cnt++ < spin_timeout);
	bugcheck("spin_timeout %x",cnt);
}