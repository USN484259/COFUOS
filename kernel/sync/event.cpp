#include "event.hpp"
#include "lock_guard.hpp"
#include "interface/include/object.hpp"

using namespace UOS;

event::event(bool initial_state) : state(initial_state ? 1 : 0) {}

event::~event(void){
	objlock.lock();
	notify(ABANDON);
}

REASON event::wait(qword us,wait_callback func){
	if (state){
		if (func){
			func();
		}
		return PASSED;
	}
	return waitable::wait(us,func);
}

bool event::signal_one(void){
	thread* ptr;
	interrupt_guard<void> ig;
	do{
		lock_guard<spin_lock> guard(objlock);
		ptr = wait_queue.get();
		if (ptr == nullptr){
			return false;
		}
	}while(0 == imp_notify(ptr,NOTIFY));
	return true;
}

size_t event::signal_all(void){
	interrupt_guard<spin_lock> guard(objlock);
	state = 1;
	guard.drop();
	return notify();
}

void event::reset(void){
	interrupt_guard<spin_lock> guard(objlock);
	state = 0;
}

bool event::relax(void){
	interrupt_guard<void> ig;
	auto res = waitable::relax();
	if (!res){
		if (named)
			named_obj.erase(this);
		delete this;
	}
	return res;
}

void event::manage(void* ptr){
	interrupt_guard<void> ig;
	if (ptr){
		if (get_reference_count() == 0)
			bugcheck("expose non-managed event @ %p",this);
		named = true;
	}
	else
		waitable::manage();
}