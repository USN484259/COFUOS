#include "types.hpp"
#include "hal.hpp"
using namespace UOS;


#ifdef _DEBUG

void kdb_init(word port){
	__outbyte(port+1,0);
	__outbyte(port+3,0x80);
	__outbyte(port+0,2);
	__outbyte(port+1,0);
	__outbyte(port+3,0x03);
	__outbyte(port+2,0xC7);
	__outbyte(port+4,0x0B);	//?????
	
	__debugbreak();
	
}


void kdb_break(qword id,qword errcode,qword* context){
	
	
	
	
	
}

#endif



extern "C"
void dispatch_exception(qword id,qword errcode,qword* context){

//special errcode indicates BugCheck call

#ifdef _DEBUG
	kdb_break(id,errcode,context);
#endif
	//TODO handle exceptions here

}



