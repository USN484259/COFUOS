#include "waitable.hpp"
#include "thread.hpp"
#include "core_state.hpp"
#include "dev/include/timer.hpp"
#include "lock_guard.hpp"
#include "process.hpp"
#include "assert.hpp"

using namespace UOS;

bool thread_queue::empty(void) const{
	if (head){
		assert(tail);
		return false;
	}
	assert(!tail);
	return true;
}

void thread_queue::put(thread* th){
	th->next = nullptr;
	if (tail){
		assert(head && tail->next == nullptr);
		tail->next = th;
		tail = th;
	}
	else{
		assert(head == nullptr);
		head = tail = th;
	}
}

thread* thread_queue::get(void){
	if (head){
		assert(tail);
		auto th = head;
		if (th->next == nullptr){
			assert(head == tail);
			tail = nullptr;
		}
		head = th->next;
		th->next = nullptr;
		
		return th;
	}
	else{
		assert(tail == nullptr);
		return nullptr;
	}
}

thread*& thread_queue::next(thread* th){
	return th->next;
}

void thread_queue::clear(void){
	head = tail = nullptr;
}

waitable::~waitable(void){
	objlock.lock();
	if(ref_count){
		bugcheck("deleting object %p with ref_count %d",this,ref_count);
	}
	if (wait_queue.head){
		bugcheck("deleting object %p while thread %p is waiting",this,wait_queue.head);
	}
}

waitable* waitable::duplicate(process*){
	return acquire() ? this : nullptr;
}

size_t waitable::notify(REASON reason){
	IF_assert;
	assert(objlock.is_locked());
	thread* ptr = wait_queue.head;
	wait_queue.clear();
	objlock.unlock();
	return imp_notify(ptr,reason);
}

REASON waitable::imp_wait(qword us){
	IF_assert;
	assert(objlock.is_locked());
	this_core core;
	thread* this_thread = core.this_thread();
	{
		if (this_thread->has_context())
			bugcheck("wait called in IRQ @ %p",this);
		if (this_thread == this)
			bugcheck("waiting on self @ %p",this);
	}
	thread* next_thread;
	thread* put_back = nullptr;
	do{
		next_thread = ready_queue.get();
		if (!next_thread)
			bugcheck("no ready thread");
		if (next_thread == this){
			// corner case: when (whatever) waits on (this), and (this) is a ready thread
			put_back = next_thread;
			continue;
		}
		next_thread->lock();
		if (next_thread->set_state(thread::RUNNING))
			break;
		next_thread->on_stop();
	}while(true);
	next_thread->put_slice(scheduler::max_slice);
	qword ticket = 0;
	this_thread->lock();
	if (us)
		ticket = timer.wait(us,on_timer,this_thread);
	if (this_thread->set_state(thread::WAITING,ticket,this)){
		this_thread->put_slice(scheduler::max_slice);
		wait_queue.put(this_thread);

	}
	else if (ticket){
		timer.cancel(ticket);
	}
	this_thread->unlock();
	if (put_back)
		ready_queue.put(put_back);
	objlock.unlock();
	core.switch_to(next_thread);
	return this_thread->get_reason();
}

REASON waitable::wait(qword us,wait_callback func){
	interrupt_guard<spin_lock> guard(objlock);
	if (func)
		func();
	guard.drop();
	return imp_wait(us);
}

void waitable::on_timer(qword ticket,void* ptr){
	IF_assert;
	auto th = (thread*)ptr;
	if (th->get_ticket() != ticket)
		return;
	th->lock();
	if (!th->set_state(thread::READY,TIMEOUT)){
		th->on_stop();
		return;
	}
	this_core core;
	auto this_thread = core.this_thread();
	if (th->get_priority() < this_thread->get_priority()){
		if (!th->set_state(thread::RUNNING))
			bugcheck("thread state corrupted @ %p",th);
		this_thread->lock();
		if (this_thread->set_state(thread::READY)){
			this_thread->put_slice(scheduler::max_slice);
			ready_queue.put(this_thread);
		}
		this_thread->unlock();
		core.switch_to(th);
	}
	else{
		ready_queue.put(th);
		th->unlock();
	}
}

//wait-timeout should happen less likely, currently O(n)
void waitable::cancel(thread* th){
	assert(th);
	interrupt_guard<spin_lock> guard(objlock);
	auto prev = wait_queue.head;
	if (prev == th){
		wait_queue.get();
		return;
	}
	while(prev){
		auto next = thread_queue::next(prev);
		if (next == th)
			break;
		prev = next;
	}
	if (prev){
		auto th_next = thread_queue::next(th);
		if (th_next){
			thread_queue::next(prev) = th_next;
		}
		else{
			assert(th == wait_queue.tail);
			thread_queue::next(prev) = nullptr;
			wait_queue.tail = prev;
		}
		return;
	}
	bugcheck("wait_queue corrupted @ %p",&wait_queue);
}

size_t waitable::imp_notify(thread* th,REASON reason){
	IF_assert;
	if (!th)
		return 0;

	size_t count = 0;
	
	while(th){
		auto next = thread_queue::next(th);
		th->lock();
		if (th->set_state(thread::READY,reason)){
			ready_queue.put(th);
			th->unlock();
			++count;
		}
		else{
			th->on_stop();
		}
		th = next;
	}
	// this_core core;
	// if (!core.this_thread()->has_context())
	// 	core_manager::preempt(false);
	return count;
}

bool waitable::acquire(void){
	interrupt_guard<spin_lock> guard(objlock);
	if (!ref_count)
		return false;
	++ref_count;
	return true;
}

bool waitable::relax(void){
	interrupt_guard<spin_lock> guard(objlock);
	if (ref_count == 0)
		bugcheck("relaxing %p without reference",this);
	return (--ref_count);
}

void waitable::manage(void*){
	interrupt_guard<spin_lock> guard(objlock);
	if (ref_count)
		bugcheck("managing object with %d references",ref_count);
	++ref_count;
}