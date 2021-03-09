#include "exception.hpp"
#include "kdb.hpp"
#include "sysinfo.hpp"
#include "intrinsics.hpp"
#include "sync/include/lock_guard.hpp"
#include "dev/include/acpi.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"

using namespace UOS;

struct PF_STATE{
	byte P : 1;
	byte W : 1;
	byte U : 1;
	byte R : 1;
	byte I : 1;
	byte : 0;
};

bool on_page_fault(qword errcode,qword va,context* context){
	PT pt;
	if (IS_HIGHADDR(va)){
		pt = vm.peek(va);
	}
	else{
		this_core core;
		pt = core.this_thread()->get_process()->vspace->peek(va);
	}
	PF_STATE& state = *(PF_STATE*)&errcode;

	if (((context->cs & 0x03) ? 1 : 0) != state.U)
		bugcheck("#PF state.U mismatch (%x,%x)",errcode,(qword)context->cs);

	do{
		if (0 == state.P){
			if (pt.present)
				bugcheck("#PF state.P mismatch (%x,%x)",errcode,*(qword*)&pt);
			else
				break;
		}
		if (state.I && state.W)
			bugcheck("#PF invalid state %x",errcode);
		
		if (state.I){
			if (pt.xd)
				break;
			if (pt.user != state.U)
				break;	
		}
		else{
			if (state.W && pt.write == 0)
				break;
			if (state.U && pt.user == 0)
				break;
		}
		invlpg((void*)va);
		return true;
	}while(false);
	return false;
}

void on_fpu_switch(context* context){
	this_core core;
	auto this_thread = core.this_thread();
	auto owner = core.fpu_owner();
	if (this_thread == owner)
		bugcheck("#NM while owning fpu @ %p",this_thread);
	clts();
	if (owner){
		owner->save_sse();
	}
	this_thread->load_sse();
	core.fpu_owner(this_thread);
}

exception::exception(void){
	features.set(decltype(features)::EXCEPT);
}

bool exception::dispatch(byte id,qword errcode,context* context){
	IF_assert;
	if (id > 0x10)
		return false;
	switch(id){
		case 0x02:	//NMI
		case 0x08:	//DF
		case 0x09:	//??
		case 0x0A:	//TS
		case 0x0F:	//??
			return false;
		case 0x0E:	//PF handler
			if (on_page_fault(errcode,read_cr2(),context))
				return true;
			break;
		case 0x07:	//NM handler
			on_fpu_switch(context);
			return true;
	}
	if ((context->cs & 0x03) != 0x03){
		assert(context->cs == SEG_KRNL_CS);
		return false;
	}
	this_core core;
	auto this_thread = core.this_thread();
	if (!this_thread->user_handler || (this_thread->user_handler & HIGHADDR(0))){
		//kill user process
		this_thread->get_process()->kill(id);
		return true;
	}
	//pass exception to user space
	context->rip = this_thread->user_handler;
	this_thread->user_handler = 0;
	return true;
}

extern "C"
void dispatch_exception(byte id,qword errcode,context* context){

//bugcheck : 0xFF
	if (features.get(decltype(features)::EXCEPT)){
		if (eh.dispatch(id,errcode,context))
			return;
	}
	if (features.get(decltype(features)::GDB)){
		//clear TF
		context->rflags &= ~0x100;
		debug_stub.get().signal(id,errcode,context);
		if (id < 20)
			return;
	}

	if (id != 0xFF){
		bugcheck("unhandled_exception #%d",id);
	}
	shutdown();
}
