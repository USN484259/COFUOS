#include "event.hpp"
#include "lock_guard.hpp"

using namespace UOS;

event::event(bool initial_state) : state(initial_state ? 1 : 0) {}

waitable::REASON event::wait(qword us){
	interrupt_guard<void> ig;
	if (state)
		return PASSED;
	lock.lock();
	return imp_wait(us);
}

bool event::signal_one(void){
	thread* ptr;
	interrupt_guard<void> ig;
	{
		lock_guard<spin_lock> guard(lock);
		ptr = wait_queue.get();
	}
	return imp_notify(ptr,NOTIFY);
}

size_t event::signal_all(void){
	interrupt_guard<void> ig;
	state = 1;
	return notify();
}

void event::reset(void){
	state = 0;
}