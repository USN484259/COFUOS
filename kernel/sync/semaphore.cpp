#include "semaphore.hpp"
#include "lock_guard.hpp"
#include "process/include/core_state.hpp"
#include "interface/include/object.hpp"
#include "assert.hpp"

using namespace UOS;

semaphore::semaphore(dword initial) : total(initial), count(initial) {
	assert(initial);
}

semaphore::~semaphore(void){
	objlock.lock();
	notify(ABANDON);
}

bool semaphore::relax(void){
	interrupt_guard<void> ig;
	auto res = waitable::relax();
	if (!res){
		if (named)
			named_obj.erase(this);
		delete this;
	}
	return res;
}

void semaphore::manage(void* ptr){
	interrupt_guard<void> ig;
	if (ptr){
		if (get_reference_count() == 0)
			bugcheck("expose non-managed semaphore @ %p",this);
		named = true;
	}
	else
		waitable::manage();
}

bool semaphore::check(void){
	interrupt_guard<spin_lock> guard(objlock);
	if (count)
		return count--;
	return false;
}

REASON semaphore::wait(qword us,wait_callback func){
	REASON reason = PASSED;
	do{
		interrupt_guard<spin_lock> guard(objlock);
		if (func){
			func();
			func = nullptr;
		}
		if (count){
			--count;
			break;
		}
		guard.drop();
		reason = imp_wait(us);
	}while(reason == NOTIFY);
	return reason;
}

bool semaphore::signal(void){
	thread* th;
	do{
		interrupt_guard<spin_lock> guard(objlock);
		if (count){
			count = min(count + 1,total);
			return false;
		}
		th = wait_queue.get();
		if (th == nullptr){
			++count;
			return false;
		}
	}while(0 == imp_notify(th,NOTIFY));
	return true;
}