#include "thread.hpp"
#include "core_state.hpp"
#include "constant.hpp"
#include "memory/include/vm.hpp"
#include "process.hpp"
#include "pe64.hpp"
#include "dev/include/timer.hpp"
#include "sync/include/lock_guard.hpp"
#include "assert.hpp"
#include "hal.hpp"

using namespace UOS;


id_gen<dword> thread::new_id;

//initial thread
thread::thread(initial_thread_tag, process* p) : id(new_id()), state(RUNNING), priority(scheduler::idle_priority), slice(scheduler::max_slice), ps(p) {
	assert(ps && id == 0);
	krnl_stk_top = 0;	//???
	krnl_stk_reserved = pe_kernel->stk_reserve;
}

thread::thread(process* p, procedure entry, void* arg, qword stk_size) : id(new_id()), priority(scheduler::kernel_priority), slice(scheduler::max_slice), ps(p), krnl_stk_reserved(align_down(stk_size,PAGE_SIZE)) {
	assert(ps && krnl_stk_reserved >= PAGE_SIZE);
	lock_guard<spin_lock> guard(rwlock);
	auto va = vm.reserve(0,krnl_stk_reserved/PAGE_SIZE);
	if (!va)
		bugcheck("vm.reserve failed with 0x%x pages",krnl_stk_reserved);
	krnl_stk_top = va + krnl_stk_reserved;
	auto res = vm.commit(krnl_stk_top - PAGE_SIZE,1);
	if (!res)
		bugcheck("vm.commit failed @ %p",krnl_stk_top - PAGE_SIZE);
	gpr.rbp = krnl_stk_top;
	gpr.rsp = krnl_stk_top - 0x20;
	gpr.ss = SEG_KRNL_SS;
	gpr.cs = SEG_KRNL_CS;
	gpr.rip = reinterpret_cast<qword>(entry);
	gpr.rflags = 0x202;		//IF
	gpr.rcx = reinterpret_cast<qword>(arg);
	dbgprint("new thread $%d @ %p",id,this);
	ready_queue.put(this);
}

thread::~thread(void){
	if (state != STOPPED)
		bugcheck("deleting non-stop thread #%d @ %p",id,this);
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
		if (!get_process()->vspace->release(user_stk_top - user_stk_reserved,user_stk_reserved/PAGE_SIZE))
			bugcheck("vspace->release failed @ %p",user_stk_top - user_stk_reserved);
	}
	if (!vm.release(krnl_stk_top - krnl_stk_reserved,krnl_stk_reserved/PAGE_SIZE))
		bugcheck("vm.release failed @ %p",krnl_stk_top - krnl_stk_reserved);
}

waitable::REASON thread::wait(qword us){
	interrupt_guard<void> ig;
	rwlock.lock();
	if (state == STOPPED){
		rwlock.unlock();
		return PASSED;
	}
	return waitable::imp_wait(us);
}

void thread::set_priority(byte val){
	assert(val < scheduler::max_priority);
	priority = val;
}

bool thread::set_state(thread::STATE st, qword arg, waitable* obj){
	IF_assert;
	assert(rwlock.is_locked());
	if (state == STOPPED){
		return false;
	}
	switch(st){
	case READY:
		if (state == WAITING){
			//arg as reason
			switch(arg){
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
	case STOPPED:
		break;
	}
	state = st;
	return true;
}

bool thread::relax(void){
	auto res = waitable::relax();
	if (!res){
		ps->erase(this);
	}
	return res;
}

void thread::on_stop(void){
	assert(state == thread::STOPPED);
	notify();
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
		next_thread->unlock();
		next_thread->on_stop();
	}while(true);
	bool esc = false;
	{
		lock_guard<thread> guard(*this_thread);
		if (us){
			auto ticket = timer.wait(us,on_timer,this_thread);
			if (!this_thread->set_state(thread::WAITING,ticket,nullptr)){
				timer.cancel(ticket);
				esc = true;
			}
		}
		else{
			if (this_thread->set_state(thread::READY))
				ready_queue.put(this_thread);
			else
				esc = true;
		}
	}
	if (esc)
		core.escape(next_thread);
	else
		core.switch_to(next_thread);
}

void thread::kill(thread* th){
	this_core core;
	thread* this_thread = core.this_thread();
	interrupt_guard<void> ig;
	{
		lock_guard<thread> guard(*th);
		th->set_state(STOPPED);
	}
	if (this_thread == th){
		core_manager::preempt();
	}
}
