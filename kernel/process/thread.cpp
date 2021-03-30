#include "thread.hpp"
#include "core_state.hpp"
#include "constant.hpp"
#include "memory/include/vm.hpp"
#include "process.hpp"
#include "pe64.hpp"
#include "dev/include/timer.hpp"
#include "lock_guard.hpp"
#include "assert.hpp"

using namespace UOS;


id_gen<dword> thread::new_id;

//initial thread
thread::thread(initial_thread_tag, process* p) : id(new_id()), state(RUNNING), hold_lock(0), priority(scheduler::idle_priority), slice(scheduler::max_slice), ps(p) {
	assert(ps && id == 0);
	krnl_stk_top = 0;	//???
	krnl_stk_reserved = pe_kernel->stk_reserve;
}

thread::thread(process* p, procedure entry, const qword* args, qword stk_size) : id(new_id()), state(READY), hold_lock(0), priority(scheduler::kernel_priority), slice(scheduler::max_slice), ps(p), krnl_stk_reserved(align_down(stk_size,PAGE_SIZE)) {
	IF_assert;
	assert(ps && krnl_stk_reserved >= PAGE_SIZE);
	acquire();
	lock_guard<spin_lock> guard(objlock);
	auto va = vm.reserve(0,2 + krnl_stk_reserved/PAGE_SIZE);
	if (!va)
		bugcheck("vm.reserve failed with 0x%x pages",krnl_stk_reserved);
	krnl_stk_top = va + PAGE_SIZE + krnl_stk_reserved;
	auto res = vm.commit(krnl_stk_top - PAGE_SIZE,1);
	if (!res)
		bugcheck("vm.commit failed @ %p",krnl_stk_top - PAGE_SIZE);
	gpr.rbp = krnl_stk_top;
	gpr.rsp = krnl_stk_top - 0x30;
	gpr.ss = SEG_KRNL_SS;
	gpr.cs = SEG_KRNL_CS;
	gpr.rip = reinterpret_cast<qword>(entry);
	gpr.rflags = 0x202;		//IF
	gpr.rcx = args[0];
	gpr.rdx = args[1];
	gpr.r8 = args[2];
	gpr.r9 = args[3];
	dbgprint("new thread $%d @ %p",id,this);
	ready_queue.put(this);

}

thread::~thread(void){
	if (state != STOPPED)
		bugcheck("deleting non-stop thread #%d @ %p",id,this);
	if (hold_lock)
		bugcheck("deleting thread #%d @ %p while holding lock %x",id,this,hold_lock);
	dbgprint("deleted thread #%d @ %p",id,this);
	if (sse){
		//flush FPU state from this core
		this_core core;
		if (core.fpu_owner() == this){
			core.fpu_owner(nullptr);
		}
		//TODO remove FPU context from all processor core
		delete sse;
	}
	if (user_stk_top){
		if (!get_process()->vspace->release(user_stk_top - user_stk_reserved - PAGE_SIZE,1 + user_stk_reserved/PAGE_SIZE))
			bugcheck("vspace->release failed @ %p",user_stk_top - user_stk_reserved);
	}
	if (!vm.release(krnl_stk_top - krnl_stk_reserved - PAGE_SIZE,2 + krnl_stk_reserved/PAGE_SIZE))
		bugcheck("vm.release failed @ %p",krnl_stk_top - krnl_stk_reserved);
}

REASON thread::wait(qword us,wait_callback func){
	if (state == STOPPED){
		if (func){
			func();
		}
		return PASSED;
	}
	return waitable::wait(us,func);
}

bool thread::set_priority(byte val){
	if (val >= scheduler::max_priority)
		return false;
	priority = val;
	return true;
}

bool thread::set_state(thread::STATE st, qword arg, waitable* obj){
	IF_assert;
	assert(objlock.is_locked());
	if (state == STOPPED){
		return false;
	}
	switch(st){
	case READY:
		if (state == WAITING){
			//arg as reason
			switch(arg){
			case REASON::ABANDON:
				assert(wait_for);
				wait_for->cancel(this);
				//fall through
			case REASON::NOTIFY:
				assert(wait_for);
				if (timer_ticket)
					timer.cancel(timer_ticket);
				break;
			case REASON::TIMEOUT:
				assert(timer_ticket);
				if (wait_for)
					wait_for->cancel(this);
				break;
			default:
				assert(false);
			}
			reason = (REASON)arg;
			wait_for = nullptr;
			timer_ticket = 0;
		}
		break;
	case RUNNING:
		assert(state == READY);
		break;
	case WAITING:
		assert(state == RUNNING);
		//arg as timer_ticket, obj as wait_for
		assert(arg || obj);
		assert(wait_for == nullptr && timer_ticket == 0);
		wait_for = obj;
		timer_ticket = arg;
		break;
	default:
		bugcheck("thread::set_state bad state @ %p",this);
	}
	state = st;
	return true;
}

bool thread::relax(void){
	interrupt_guard<void> ig;
	auto res = waitable::relax();
	if (!res){
		ps->erase(this);
	}
	return res;
}

void thread::manage(void*){
	IF_assert;
	waitable::manage();
	++ref_count;
	assert(ref_count == 2);
}

void thread::on_stop(void){
	IF_assert;
	assert(objlock.is_locked());
	assert(state == thread::STOPPED);

	assert(wait_for == nullptr && timer_ticket == 0);

	if (hold_lock){
		if (hold_lock & HOLD_VSPACE){
			auto vspace = get_process()->vspace;
			assert(vspace->is_locked() && !vspace->is_exclusive());
			vspace->unlock();
		}
		if (hold_lock & HOLD_HANDLE){
			auto& ht = get_process()->handles;
			assert(ht.is_locked() && !ht.is_exclusive());
			ht.unlock();
		}
		hold_lock = 0;
	}
	notify();
	ps->on_exit();
	relax();
}

void thread::save_sse(void){
	if (!sse){
		sse = new SSE_context;
	}
	fxsave(sse);
}

void thread::load_sse(void){
	if (sse){
		fxrstor(sse);
	}
	else{
		fpu_init();
	}
}

void thread::hold(byte val){
	IF_assert;
	assert(is_locked());
	assert(val == HOLD_HANDLE || val == HOLD_VSPACE);
	assert(0 == (hold_lock & val));
	hold_lock |= val;
}

void thread::drop(byte val){
	IF_assert;
	assert(is_locked());
	assert(val == HOLD_HANDLE || val == HOLD_VSPACE);
	assert(hold_lock & val);
	hold_lock &= (~val);
}

void thread::sleep(qword us){
	this_core core;
	thread* this_thread = core.this_thread();
	thread* next_thread;
	interrupt_guard<void> ig;
	do{
		next_thread = ready_queue.get();
		if (!next_thread)
			bugcheck("no ready thread");
		next_thread->lock();
		if (next_thread->set_state(thread::RUNNING))
			break;
		next_thread->on_stop();
	}while(true);
	bool res;
	this_thread->lock();
	if (us){
		auto ticket = timer.wait(us,on_timer,this_thread);
		res = this_thread->set_state(thread::WAITING,ticket,nullptr);
		if (!res){
			timer.cancel(ticket);
		}
	}
	else{
		res = this_thread->set_state(thread::READY);
		if (res){
			this_thread->put_slice(scheduler::max_slice);
			ready_queue.put(this_thread);
		}
	}
	this_thread->unlock();
	if (res){
		core.switch_to(next_thread);
	}
	else{
		core.escape(next_thread);
	}	
}

void thread::kill(thread* th){
	this_core core;
	thread* this_thread = core.this_thread();
	bool kill_self = (this_thread == th);
	interrupt_guard<void> ig;
	{
		lock_guard<thread> guard(*th);
		//th->set_state(STOPPED);
		auto state = th->state;
		th->state = STOPPED;
		if (state == WAITING){
			if (th->wait_for){
				th->wait_for->cancel(th);
				th->wait_for = nullptr;
			}
			if (th->timer_ticket){
				timer.cancel(th->timer_ticket);
				th->timer_ticket = 0;
			}
			if (!kill_self){
				guard.drop();
				th->on_stop();
			}
		}
	}
	if (kill_self){
		core_manager::preempt(true);
	}
}
