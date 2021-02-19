#include "core_state.hpp"
#include "util.hpp"
//#include "hal.hpp"
#include "dev/include/acpi.hpp"
#include "cpu/include/apic.hpp"
#include "memory/include/vm.hpp"
#include "process.hpp"
#include "dev/include/timer.hpp"
#include "sync/include/lock_guard.hpp"
#include "assert.hpp"
#include "lang.hpp"

using namespace UOS;

constexpr dword scheduler::max_slice;
constexpr word scheduler::max_priority;

thread* scheduler::get(word level){
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
	core_list = new core_state[count];
	auto& self = core_list[0];
	self.uid = apic.id();
	self.slice = 0;
	self.gc_count = 0;
	auto th = proc.get_initial_thread();
	self.this_thread = th;
	self.fpu_owner = th;
	self.gc_base = 0;
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
	//dbgprint("%d --> %d", this_thread->get_id(), next_thread ? next_thread->get_id() : this_thread->get_id());
	if (next_thread){
		this_thread->set_state(thread::READY);
		ready_queue.put(this_thread);
		core.switch_to(next_thread);
	}
	else{
		core.slice(scheduler::max_slice);
	}
}

#ifndef NDEBUG
this_core::this_core(void){
	auto gs_value = reinterpret_cast<core_state*>(rdmsr(MSR_GS_BASE));
	assert(gs_value && gs_value == cores.get());
}
#endif

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

void this_core::gc_service(void){
	qword vbase = read_gs<qword>(offsetof(core_state,gc_base));
	if (vbase){
		dword pagecount = read_gs<dword>(offsetof(core_state,gc_count));
		auto res = vm.release(vbase,pagecount);
		if (!res)
			bugcheck("gc failed @ %p with %d pages",vbase,pagecount);
		dbgprint("gc released %p",vbase);
		write_gs<qword>(offsetof(core_state,gc_base),0);
	}
}

void this_core::switch_to(thread* th){
	IF_assert;
	gc_service();
	assert(th && th->has_context());
	assert(this_thread()->get_state() != thread::RUNNING);
	//dbgprint("%d --> %d from %p",this_thread()->get_id(),th->get_id(),return_address());
	th->set_state(thread::RUNNING);
	slice(scheduler::max_slice);
	if (this_thread()->has_context()){
		irq_switch_to(0,reinterpret_cast<void*>(th));
	}
	else{
		context_trap(reinterpret_cast<qword>(th));
	}
}

void this_core::escape(bool has_context,qword vbase,dword pagecount){
	IF_assert;
	gc_service();
	thread* th = ready_queue.get();
	assert(th && th->has_context());
	th->set_state(thread::RUNNING);
	slice(scheduler::max_slice);
	write_gs(offsetof(core_state,gc_base),vbase);
	write_gs(offsetof(core_state,gc_count),pagecount);
	if (has_context){
		irq_switch_to(0,reinterpret_cast<void*>(th));
	}
	else{
		context_trap(reinterpret_cast<qword>(th));
	}
	bugcheck("this_core::escape failed");
}