#include "mutex.hpp"
#include "process/include/core_state.hpp"
#include "lang.hpp"
#include "lock_guard.hpp"
#include "assert.hpp"
#include "intrinsics.hpp"

using namespace UOS;


mutex::~mutex(void){
	if (owner)
		bugcheck("mutex owned on destruct @ %p",this);
}


bool mutex::try_lock(void){
	this_core core;
	thread* this_thread = core.this_thread();
	return (cmpxchg_ptr(&owner,this_thread,(thread*)nullptr) == nullptr);
}

void mutex::lock(void){
	wait();
}

void mutex::unlock(void){
	this_core core;
	thread* this_thread = core.this_thread();
	if (owner != this_thread)
		bugcheck("mutex corrupted @ %p",this);
	owner = nullptr;
	notify();
}

REASON mutex::wait(qword us,wait_callback func){
	this_core core;
	thread* this_thread = core.this_thread();
	REASON reason = PASSED;
	interrupt_guard<void> ig;
	do{
		objlock.lock();
		if (func){
			func();
			func = nullptr;
		}
		if (cmpxchg_ptr(&owner,this_thread,(thread*)nullptr) == nullptr){
			assert(owner == this_thread);
			objlock.unlock();
			break;
		}
		reason = imp_wait(us);
	}while(reason == NOTIFY);
	return reason;
}

size_t mutex::notify(void){
	thread* ptr;
	interrupt_guard<void> ig;
	{
		lock_guard<spin_lock> guard(objlock);
		ptr = wait_queue.get();
	}
	return imp_notify(ptr);
}