#include "exception.hpp"
#include "kdb.hpp"
#include "sysinfo.hpp"
#include "intrinsics.hpp"
#include "sync/include/lock_guard.hpp"
#include "dev/include/acpi.hpp"

using namespace UOS;

exception::exception(void){
	features.set(decltype(features)::EXCEPT);
}

void exception::push(byte id,CALLBACK callback,void* data){
	if (id >= 20){
		bugcheck("invalid ID %d",id);
	}
	interrupt_guard<spin_lock> guard(lock);
	table[id].push(handler{callback,data});
}

exception::CALLBACK exception::pop(byte id){
	if (id >= 20){
		bugcheck("invalid ID %d",id);
	}
	interrupt_guard<spin_lock> guard(lock);
	if (table[id].empty()){
		bugcheck("pop from empty stack @ %p",&table[id]);
	}
	auto ret = table[id].top().callback;
	table[id].pop();
	return ret;
}

bool exception::dispatch(byte id,qword errcode,context* context){
	if (id >= 20)
		return false;

	interrupt_guard<spin_lock> guard(lock);
	for (auto it : table[id]){
		if (it.callback(id,errcode,context,it.data))
			return true;
	}
	return false;
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
