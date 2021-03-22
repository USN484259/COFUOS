#include "rwlock.hpp"
#include "lock_guard.hpp"
#include "process/include/core_state.hpp"
#include "assert.hpp"

using namespace UOS;

rwlock::rwlock(void){
	ref_count = 0;
}

rwlock::~rwlock(void){
	{
		interrupt_guard<spin_lock> guard(objlock);
		if (owner || share_count)
			bugcheck("deleting active rwlock (%p,%d)",owner,share_count);
	}
}

bool rwlock::try_lock(MODE mode){
	interrupt_guard<spin_lock> guard(objlock);
	if (mode == MODE::EXCLUSIVE){
		if (owner == nullptr && share_count == 0){
			this_core core;
			owner = core.this_thread();
			return true;
		}
		return false;
	}
	else{
		if (owner)
			return false;
		++share_count;
		return true;
	}
}

void rwlock::lock(MODE mode){
	do{
		interrupt_guard<spin_lock> guard(objlock);
		if (mode == MODE::EXCLUSIVE){
			if (owner == nullptr && share_count == 0){
				this_core core;
				owner = core.this_thread();
				return;
			}
		}
		else{
			if (owner == nullptr){
				++share_count;
				return;
			}
		}
		guard.drop();
	}while(imp_wait(0) == NOTIFY);
	bugcheck("locking deleted rwlock @ %p",this);
}

void rwlock::unlock(void){
	interrupt_guard<spin_lock> guard(objlock);
	if (owner){
		assert(share_count == 0);
		this_core core;
		if (owner != core.this_thread())
			bugcheck("releasing non-owning exclusive lock @ %p",this);

		owner = nullptr;
	}
	else{
		if (share_count == 0)
			bugcheck("releasing free shared lock @ %p",this);
		if (--share_count != 0)
			return;
	}
	guard.drop();
	notify();
}

bool rwlock::is_locked(void) const{
	return (owner || share_count);
}

bool rwlock::is_exclusive(void) const{
	return owner;
}