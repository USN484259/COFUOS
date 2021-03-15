#include "event.hpp"
#include "lock_guard.hpp"

using namespace UOS;

event::event(bool initial_state) : state(initial_state ? 1 : 0) {}

REASON event::wait(qword us,wait_callback func){
	if (state){
		if (func)
			func();
		return PASSED;
	}
	return waitable::wait(us,func);
}

bool event::signal_one(void){
	thread* ptr;
	interrupt_guard<void> ig;
	{
		lock_guard<spin_lock> guard(objlock);
		ptr = wait_queue.get();
	}
	return imp_notify(ptr);
}

size_t event::signal_all(void){
	interrupt_guard<void> ig;
	state = 1;
	return notify();
}

void event::reset(void){
	state = 0;
}