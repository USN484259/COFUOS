#include "lock.hpp"
#include "assert.hpp"
#include "atomic.hpp"



using namespace UOS;



interrupt_guard::interrupt_guard(void) : state(__readeflags() & 0x0200){
	assert(0==(state & ~0x0200));
	_disable();
}

interrupt_guard::~interrupt_guard(void){
	assert(0==(state & ~0x0200));
	if (state)
		_enable();
}


mutex::mutex(void) : m(0){}

void mutex::lock(void)volatile{
	while(cmpxchg<dword>(&m,1,0))
		_mm_pause();
	
	assert(1,m);

	
}

void mutex::unlock(void)volatile{
	assert(1 == m);
	
	dword tmp=xchg<dword>(&m,0);
	assert(1 == tmp);
	
}

bool mutex::lock_state(void) const volatile{
	return m ? true : false;
	
}