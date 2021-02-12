#include "waitable.hpp"
#include "thread.hpp"
#include "core_state.hpp"
#include "dev/include/timer.hpp"
#include "sync/include/lock_guard.hpp"
#include "assert.hpp"

using namespace UOS;


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

void thread_queue::clear(void){
	head = tail = nullptr;
}

waitable::~waitable(void){
	lock.lock();
	imp_notify(wait_queue.head);
}

void waitable::wait(qword us){
	this_core core;
	thread* this_thread = core.this_thread();
	if (us){
		this_thread->timer_ticket = timer.wait(us,on_timer,this_thread);
	}
	else
		this_thread->timer_ticket = 0;

	this_thread->wait_for = this;
	this_thread->set_state(thread::WAITING);
	{
		interrupt_guard<spin_lock> guard(lock);
		wait_queue.put(this_thread);
	}
	core.switch_to(ready_queue.get());
}

void waitable::on_timer(qword ticket,void* ptr){
	IF_assert;
	auto th = (thread*)ptr;
	if (th->timer_ticket != ticket)
		return;
	assert(th->state == thread::WAITING);
	if (th->wait_for)
		th->wait_for->cancel(th);
	
	th->timer_ticket = 0;
	th->set_state(thread::READY);
	
	this_core core;
	auto this_thread = core.this_thread();
	if (th->priority < this_thread->get_priority()){
		this_thread->set_state(thread::READY);
		ready_queue.put(this_thread);
		core.switch_to(th);
	}
	else{
		ready_queue.put(th);
	}
}

//wait-timeout should happen less likely, currently O(n)
void waitable::cancel(thread* th){
	assert(th && th->wait_for == this);
	th->wait_for = nullptr;
	interrupt_guard<spin_lock> guard(lock);
	auto prev = wait_queue.head;
	if (prev == th){
		wait_queue.get();
		return;
	}
	while(prev){
		if (prev->next == prev)
			break;
		prev = prev->next;
	}
	if (prev){
		if (th->next){
			prev->next = th->next;
		}
		else{
			assert(th == wait_queue.tail);
			prev->next = nullptr;
			wait_queue.tail = prev;
		}
		return;
	}
	bugcheck("wait_queue corrupted @ %p",&wait_queue);
}

size_t waitable::imp_notify(thread* th){
	if (!th)
		return 0;
	thread* next = nullptr;
	this_core core;
	size_t count = 0;
	word cur_priority = core.this_thread()->get_priority();

	while(th){
		if (th->timer_ticket){
			timer.cancel(th->timer_ticket);
			th->timer_ticket = 0;
		}
		th->wait_for = nullptr;
		th->set_state(thread::READY);
		if (th->priority < cur_priority){
			if (next){
				if (th->priority < next->priority){
					ready_queue.put(next);
					next = th;
				}
				else{
					ready_queue.put(th);
				}
			}
			else
				next = th;
		}
		++count;
		th = th->next;
	}

	if (next){
		thread* this_thread = core.this_thread();
		this_thread->set_state(thread::READY);
		ready_queue.put(this_thread);
		core.switch_to(next);
	}
	return count;
}

size_t waitable::notify(void){
	thread* ptr;
	{
		interrupt_guard<spin_lock> guard(lock);
		ptr = wait_queue.head;
		wait_queue.clear();
	}
#ifndef _NDEBUG
	{
		auto chk = ptr;
		while(chk){
			assert(chk->state == thread::WAITING && chk->wait_for == this);
			chk = chk->next;
		}
	}
#endif
	return imp_notify(ptr);
}