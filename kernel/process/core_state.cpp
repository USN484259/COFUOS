#include "core_state.hpp"
#include "util.hpp"
//#include "hal.hpp"
#include "dev/include/acpi.hpp"
#include "cpu/include/apic.hpp"
#include "process.hpp"
#include "dev/include/timer.hpp"
#include "sync/include/lock_guard.hpp"
#include "assert.hpp"
#include "intrinsics.hpp"

using namespace UOS;

constexpr dword scheduler::max_slice;
constexpr word scheduler::max_priority;

thread* scheduler::get(word level){
	level = min(level,max_priority);
	interrupt_guard<spin_lock> guard(lock);
	for (unsigned i = 0;i < level;++i){
		auto th = ready_queue[i].get();
		if (th)
			return th;
	}
	return nullptr;
}

void scheduler::put(thread* th){
	word index = th->priority;
	assert(index < max_priority);
	interrupt_guard<spin_lock> guard(lock);
	ready_queue[index].put(th);
}

core_manager::core_manager(void){
	auto& madt = acpi.get_madt();
	count = madt.processors.size();
	assert(count);
	core_list = new core_state[count];
	auto& self = core_list[0];
	self.uid = apic.id();
	self.slice = 0;
	auto th = proc.get_initial_thread();
	self.this_thread = th;
	self.fpu_owner = th;
	wrmsr(MSR_GS_BASE,(qword)(core_list + 0));

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
	auto slice = core.slice();
	if (slice){
		core.slice(slice - 1);
		return;
	}
	auto this_thread = core.this_thread();
	assert(this_thread->has_context());
	auto next_thread = ready_queue.get(this_thread->get_priority() + 1);
	dbgprint("next thread : %p",next_thread ? next_thread : this_thread);
	if (next_thread){
		ready_queue.put(this_thread);
		core.switch_to(next_thread);
	}
	else{
		core.slice(scheduler::max_slice);
	}
}

this_core::this_core(void){
#ifndef NDEBUG
	auto gs_value = reinterpret_cast<core_state*>(rdmsr(MSR_GS_BASE));
	assert(gs_value && gs_value == cores.get());
#endif
}
word this_core::id(void){
	return read_gs<word>(offsetof(core_state,uid));
}
word this_core::slice(void){
	return read_gs<word>(offsetof(core_state,slice));
}
void this_core::slice(word val){
	write_gs(offsetof(core_state,slice),val);
}
thread* this_core::this_thread(void){
	return reinterpret_cast<thread*>(
		read_gs<qword>(offsetof(core_state,this_thread))
	);
}
thread* this_core::fpu_owner(void){
	return reinterpret_cast<thread*>(
		read_gs<qword>(offsetof(core_state,fpu_owner))
	);
}
void this_core::fpu_owner(thread* th){
	write_gs(offsetof(core_state,fpu_owner),
		reinterpret_cast<qword>(th)
	);
}

void this_core::irq_switch_to(byte,void* data){
	qword target;
	if (data){
		target = reinterpret_cast<qword>(data);
	}
	else{
		thread* th = reinterpret_cast<thread*>(
			read_gs<qword>(offsetof(core_state,this_thread))
		);
		assert(th->has_context());
		target = th->get_context()->rcx;
	}
	write_gs(offsetof(core_state,this_thread),target);
}

inline void context_trap(qword data){
	__asm__ volatile (
		"int %0"
		:
		: "i" (APIC::IRQ_CONTEXT_TRAP), "c" (data)
	);
}

void this_core::switch_to(thread* th){
	assert(th && th->has_context());
	th->set_state(thread::RUNNING);
	slice(scheduler::max_slice);
	if (this_thread()->has_context()){
		IF_assert;
		irq_switch_to(0,reinterpret_cast<void*>(th));
	}
	else{
		context_trap(reinterpret_cast<qword>(th));
	}
}
