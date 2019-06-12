#include "lock.hpp"
#include "assert.hpp"
using namespace UOS;



interrupt_guard::interrupt_guard(void) : state(__readeflags() & 0x0200){
	__assume(0==(state & ~0x0200));
	_disable();
}

interrupt_guard::~interrupt_guard(void){
	__assume(0==(state & ~0x0200));
	if (state)
		_enable();
}


mutex::mutex(void) : m(0){}

void mutex::lock(void){
	while(_InterlockedCompareExchange(&m,1,0))
		_mm_pause();
	
	assert(1,m);

	
}

void mutex::unlock(void){
	assert(1,m);
	
	dword tmp=_InterlockedExchange(&m,0);
	assert(1,tmp);
	
}

bool mutex::lock_state(void) const{
	return m ? true : false;
	
}