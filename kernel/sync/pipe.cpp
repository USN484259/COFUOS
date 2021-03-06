#include "pipe.hpp"
#include "process/include/process.hpp"
#include "lock_guard.hpp"
#include "interface/include/object.hpp"

using namespace UOS;

pipe::pipe(dword size,byte m) : \
	owner(this_core().this_thread()->get_process()),\
	mode(m), limit(size),\
	buffer((byte*)operator new(size)) {
		assert(size);
	}

pipe::~pipe(void){
	interrupt_guard<spin_lock> guard(objlock);
	operator delete(buffer,limit);
	guard.drop();
	notify(ABANDON);
}

bool pipe::relax(void){
	interrupt_guard<void> ig;
	auto res = stream::relax();
	if (!res){
		if (named)
			named_obj.erase(this);
		delete this;
	}
	return res;
}

void pipe::manage(void* ptr){
	interrupt_guard<void> ig;
	if (ptr){
		if (get_reference_count() == 0)
			bugcheck("expose non-managed pipe @ %p",this);
		named = true;
	}
	else
		stream::manage();
}

inline bool pipe::is_owner(void) const{
	this_core core;
	auto this_thread = core.this_thread();
	auto this_process = this_thread->has_context() ? proc.find(0,false) : this_thread->get_process();
	assert(this_process);
	return this_process == owner;
}

inline bool pipe::is_full(void) const{
	return (tail + 1) % limit == head;
}

inline bool pipe::is_empty(void) const{
	return head == tail;
}

inline bool pipe::check(void){
	bool is_owner_write = (mode & owner_write);
	if (is_owner() == is_owner_write)	//is_full
		return !is_full();
	else	//is_empty
		return !is_empty();
}

REASON pipe::wait(qword us,wait_callback func){
	interrupt_guard<spin_lock> guard(objlock);
	if (func){
		func();
	}
	if (check()){
		return PASSED;
	}
	guard.drop();
	return imp_wait(us);
}

dword pipe::read(void* dst,dword length){
	interrupt_guard<spin_lock> guard(objlock);
	iostate = 0;
	bool is_owner_write = (mode & owner_write);
	if (is_owner() == is_owner_write){
		//should write, fail
		iostate = OP_FAILURE;
		return 0;
	}
	dword count = 0;
	//interrupt_guard<spin_lock> guard(objlock);
	bool need_notify = is_full();
	while(count < length){
		if (is_empty())
			break;
		((byte*)dst)[count++] = buffer[head++];
		if (head == limit)
			head = 0;
	}
	if (count && need_notify){
		guard.drop();
		notify();
	}
	return count;
}

dword pipe::write(void const* sor,dword length){
	interrupt_guard<spin_lock> guard(objlock);
	iostate = 0;
	bool is_owner_write = (mode & owner_write);
	if (is_owner() != is_owner_write){
		//should read, fail
		iostate |= OP_FAILURE;
		return 0;
	}
	dword count = 0;
	//interrupt_guard<spin_lock> guard(objlock);
	if (mode & atomic_write){
		auto available = (head + limit - tail - 1) % limit;
		if (available < length)
			return 0;
	}
	bool need_notify = is_empty();
	while(count < length){
		if (is_full())
			break;
		buffer[tail++] = ((byte const*)sor)[count++];
		if (tail == limit)
			tail = 0;
	}
	assert(0 == (mode & atomic_write) || count == length);

	if (count && need_notify){
		guard.drop();
		notify();
	}
	return count;
}
