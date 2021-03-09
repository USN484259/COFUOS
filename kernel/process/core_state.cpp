#include "core_state.hpp"
#include "util.hpp"
#include "hal.hpp"
#include "dev/include/acpi.hpp"
#include "dev/include/apic.hpp"
#include "memory/include/vm.hpp"
#include "process.hpp"
#include "dev/include/timer.hpp"
#include "sync/include/lock_guard.hpp"
#include "assert.hpp"
#include "lang.hpp"

using namespace UOS;

constexpr word scheduler::max_slice;
constexpr byte scheduler::max_priority;

thread* scheduler::get(byte level){
	level = min(level,max_priority);
	interrupt_guard<spin_lock> guard(lock);
	for (unsigned i = 0;i < level;++i){
		auto th = ready_queue[i].get();
		if (th){
			//dbgprint("selected thread#%d(%d) from %p",th->get_id(),th->get_priority(),return_address());
			return th;
		}
	}
	//dbgprint("selected thread<none> from %p",return_address());
	return nullptr;
}

void scheduler::put(thread* th){
	assert(th && th->is_locked());
	word index = th->priority;
	assert(index < max_priority);
	interrupt_guard<spin_lock> guard(lock);
	ready_queue[index].put(th);
	//dbgprint("queued thread#%d(%d) from %p",th->get_id(),index,return_address());
}

core_manager::core_manager(void){
	auto& madt = acpi.get_madt();
	count = madt.processors.size();
	assert(count);
	pm.set_mp_count(count);
	core_list = new core_state[count];
	//set up core_state for BSP
	auto& self = core_list[0];
	self.uid = apic.id();
	auto th = proc.get_initial_thread();
	self.this_thread = th;
	self.fpu_owner = th;
	self.gc_ptr = nullptr;
	wrmsr(MSR_GS_BASE,(qword)(core_list + 0));
	//set up SYSCALL for BSP
	qword STAR_value = (qword)(((SEG_USER_CS - 16) << 16) | (SEG_KRNL_CS)) << 32;
	qword SFMASK_value = 0x700;	//masks IF & DF & TF (to protect GSBASE)
	wrmsr(MSR_STAR,STAR_value);
	wrmsr(MSR_LSTAR,(qword)service_entry);
	wrmsr(MSR_SFMASK,SFMASK_value);

	apic.set(APIC::IRQ_CONTEXT_TRAP,this_core::irq_switch_to,nullptr);

	//set up periodic timer here
	timer_ticket = timer.wait(scheduler::slice_us,on_timer,this,true);
}

core_state* core_manager::get(void){
	auto id = apic.id();
	for (unsigned i = 0;i < count;++i){
		if (core_list[i].uid == id)
			return core_list + i;
	}
	return nullptr;
}

void core_manager::on_timer(qword ticket,void* ptr){
	IF_assert;
	auto self = (core_manager*)ptr;
	if (self->timer_ticket != ticket)
		bugcheck("core_manager ticket mismatch (%x,%x)",ticket,self->timer_ticket);
	this_core core;
	auto this_thread = core.this_thread();
	assert(this_thread->has_context());
	if (this_thread->get_state() != thread::STOPPED){
		auto slice = this_thread->get_slice();
		assert(slice <= scheduler::max_slice);
		if (slice){
			this_thread->put_slice(slice - 1);
			return;
		}
		else{
			this_thread->put_slice(scheduler::max_slice);
		}
	}
	preempt();
}

void core_manager::preempt(void){
	IF_assert;
	this_core core;
	auto this_thread = core.this_thread();
	thread* next_thread;
	do{
		next_thread = ready_queue.get(this_thread->get_priority() + 1);
		if (!next_thread)
			break;
		next_thread->lock();
		if (next_thread->set_state(thread::RUNNING))
			break;
		next_thread->unlock();
		next_thread->on_stop();
	}while(true);
	//dbgprint("%d --> %d", this_thread->get_id(), next_thread ? next_thread->get_id() : this_thread->get_id());
	if (next_thread){
		assert(next_thread->is_locked());
		this_thread->lock();
		if (this_thread->set_state(thread::READY)){
			ready_queue.put(this_thread);
			this_thread->unlock();
			core.switch_to(next_thread);
		}
		else{
			this_thread->unlock();
			core.escape(next_thread);
		}
	}
	else if (this_thread->get_state() == thread::STOPPED){
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
		core.escape(next_thread);
	}
}

#ifndef NDEBUG
this_core::this_core(void){
	auto gs_value = reinterpret_cast<core_state*>(rdmsr(MSR_GS_BASE));
	assert(gs_value && gs_value == cores.get());
}
#endif

void this_core::irq_switch_to(byte,void* data){
	thread* cur_thread = reinterpret_cast<thread*>(
			read_gs<qword>(offsetof(core_state,this_thread))
	);
	assert(cur_thread->has_context());

	thread* target;
	if (data){
		target = reinterpret_cast<thread*>(data);
	}
	else{
		target = reinterpret_cast<thread*>(cur_thread->get_context()->rcx);
	}
	assert(target->is_locked());
	process* ps = target->get_process();
	if (cur_thread->get_process() != ps){
		//set CR0.TS
		auto cr0 = read_cr0();
		write_cr0(cr0 | 0x08);
		//change CR3
		write_cr3(ps->vspace->get_cr3());
	}
	write_gs(offsetof(core_state,this_thread),reinterpret_cast<qword>(target));
	target->unlock();
}

inline void context_trap(qword data){
	__asm__ volatile (
		"int %0"
		:
		: "i" (APIC::IRQ_CONTEXT_TRAP), "c" (data)
	);
}

void this_core::gc_service(void){
	auto gc_thread = reinterpret_cast<thread*>(
			read_gs<qword>(offsetof(core_state,gc_ptr))
		);
	if (gc_thread){
		gc_thread->on_stop();
		dbgprint("gc relaxed %p",gc_thread);
		write_gs<qword>(offsetof(core_state,gc_ptr),0);
	}
}

void this_core::switch_to(thread* th){
	IF_assert;
	gc_service();
	assert(th && th->has_context());
	assert(th->get_state() == thread::RUNNING);
	assert(this_thread()->get_state() != thread::RUNNING);
	//dbgprint("%d --> %d from %p",this_thread()->get_id(),th->get_id(),return_address());
	//th->set_state(thread::RUNNING);
	if (th->get_slice() < scheduler::min_slice)
		th->put_slice(scheduler::max_slice);
	if (this_thread()->has_context()){
		irq_switch_to(0,reinterpret_cast<void*>(th));
	}
	else{
		context_trap(reinterpret_cast<qword>(th));
	}
}

void this_core::escape(thread* th){
	IF_assert;
	gc_service();
	assert(th && th->has_context());
	assert(th->get_state() == thread::RUNNING);
	assert(this_thread()->get_state() == thread::STOPPED);
	//th->set_state(thread::RUNNING);
	write_gs(offsetof(core_state,gc_ptr),this_thread());
	if (th->get_slice() < scheduler::min_slice)
		th->put_slice(scheduler::max_slice);
	if (this_thread()->has_context()){
		irq_switch_to(0,reinterpret_cast<void*>(th));
	}
	else{
		context_trap(reinterpret_cast<qword>(th));
	}
}