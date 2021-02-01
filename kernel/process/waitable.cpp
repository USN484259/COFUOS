#include "waitable.hpp"
#include "thread.hpp"
#include "core_state.hpp"
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
		//th->next = nullptr;
		//intended, link kept
		return th;
	}
	else{
		assert(tail == nullptr);
		return nullptr;
	}
}

waitable::~waitable(void){
	if (wait_queue.head)
		notify(wait_queue.head);
}

void waitable::wait(void){
	this_core core;
	thread* this_thread = core.this_thread();
	this_thread->set_state(thread::WAITING);
	interrupt_guard<spin_lock> guard(lock);
	wait_queue.put(this_thread);
	core.switch_to(ready_queue.get());
}

void waitable::notify(thread* th){
	assert(th);
	thread* next = nullptr;
	this_core core;
	word cur_priority = core.this_thread()->get_priority();
	{
		interrupt_guard<spin_lock> guard(lock);
		while(th){
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
			th = th->next;
		}
	}
	if (next){
		thread* this_thread = core.this_thread();
		this_thread->set_state(thread::READY);
		ready_queue.put(this_thread);
		core.switch_to(next);
	}
}
