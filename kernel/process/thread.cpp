#include "thread.hpp"
#include "core_state.hpp"
#include "constant.hpp"
#include "memory/include/vm.hpp"
#include "process.hpp"
#include "pe64.hpp"
#include "dev/include/timer.hpp"
#include "sync/include/lock_guard.hpp"
#include "assert.hpp"
using namespace UOS;


id_gen<dword> thread::new_id;

//initial thread
thread::thread(initial_thread_tag, process* p) : id(new_id()), state(RUNNING), priority(scheduler::idle_priority), ps(p) {
	assert(ps && id == 0);
	krnl_stk_top = 0;	//???
	krnl_stk_reserved = pe_kernel->stk_reserve;
}

thread::thread(process* p, procedure entry, void* arg, qword stk_size) : id(new_id()), priority(scheduler::kernel_priority), ps(p), krnl_stk_reserved(align_down(stk_size,PAGE_SIZE)) {
	assert(ps && krnl_stk_reserved >= PAGE_SIZE);
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
	dbgprint("delete thread #%d @ %p",id,this);
	if (sse){
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

void thread::set_priority(word val){
	assert(val < scheduler::max_priority);
	priority = val;
}

void thread::set_state(thread::STATE st, qword arg, waitable* obj){
	IF_assert;
	switch(st){
	case READY:	
		assert(state != STOPPED);
		if (state == WAITING){
			//arg as reason
			switch(arg){
			case REASON::NOTIFY:
			case REASON::ABANDONED:
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
	case RUNNING:	//calls from switch_to
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
		assert(state == RUNNING);
		break;
	}
	state = st;
}

bool thread::relax(void){
	auto res = waitable::relax();
	if (!res){
		ps->erase(this);
	}
	return res;
}

void thread::sleep(qword us){
	this_core core;
	thread* this_thread = core.this_thread();
	interrupt_guard<void> ig;
	if (us){
		auto ticket = timer.wait(us,on_timer,this_thread);
		this_thread->set_state(thread::WAITING,ticket,nullptr);
	}
	else{
		this_thread->set_state(thread::READY);
		ready_queue.put(this_thread);
	}
	core.switch_to(ready_queue.get());
}

void thread::exit(void){
	this_core core;
	thread* this_thread = core.this_thread();
	cli();
	this_thread->set_state(STOPPED);
	assert(this_thread->krnl_stk_top && this_thread->krnl_stk_reserved);
	core.escape();
}