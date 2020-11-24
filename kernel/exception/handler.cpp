#include "types.hpp"
#include "kdb.hpp"
#include "..\cpu\include\hal.hpp"
#include "..\cpu\include\pio.hpp"
#include "..\process\include\context.hpp"
#include "util.hpp"
#include "stopcode.hpp"

#undef assert

using namespace UOS;


extern "C"
int _purecall(void){
	BugCheck(assert_failed);
}


extern "C"
void dispatch_exception(byte id,exception_context* context){

//special errcode indicates BugCheck call
	//lock_guard<volatile MP> lck(*mp);
	//mp->sync(MP::CMD::pause);

	//clear TF
	context->rflags &= ~0x100;
		
	if (kdb_enable){
		kdb_break(id,context);
		if (!kdb_enable){
			io_write<word>(0xB004, 0x2000);
			__halt();
		}
	}
	else{
		if (id!=1 && id!=3 && id!=0xFF)
			BugCheck(/*unhandled_exception*/not_implemented,id);
	}

	//mp->sync(MP::CMD::resume);
	//TODO handle exceptions here

}


