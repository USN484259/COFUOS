#include "exception.hpp"
#include "kdb.hpp"
#include "hal.hpp"
#include "port_io.hpp"

using namespace UOS;

void exception::push(byte id,CALLBACK callback,qword data){
	if (id >= 20){
		BugCheck(out_of_range,id);
	}
	table[id].push(handler{callback,data});
}

exception::CALLBACK exception::pop(byte id){
	if (id >= 20){
		BugCheck(out_of_range,id);
	}
	if (table[id].empty()){
		BugCheck(corrupted,table[id]);
	}
	auto ret = table[id].top().callback;
	table[id].pop();
	return ret;
}

bool exception::dispatch(byte id,exception_context* context){
	if (id >= 20)
		return false;
	for (auto it : table[id]){
		if (it.callback(context,it.data))
			return true;
	}
	return false;
}

extern "C"
void dispatch_exception(byte id,exception_context* context){

//BugCheck : 0xFF

	if (id < 20 && eh.dispatch(id,context))
		return;

	if (kdb_enable){
		//clear TF
		context->rflags &= ~0x100;
		kdb_break(id,context);
		if (kdb_enable)
			return;
	}
	if (id != 0xFF){
		BugCheck(unhandled_exception,id);
	}
	while (true){
		port_write(0xB004, (word)0x2000);
		__halt();
	}
}
