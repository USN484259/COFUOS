#include "types.hpp"
#include "hal.hpp"
using namespace UOS;


#ifdef _DEBUG

void kdb_init(word port){
	io_outb(port+1,0);
	io_outb(port+3,0x80);
	io_outb(port+0,2);
	io_outb(port+1,0);
	io_outb(port+3,0x03);
	io_outb(port+2,0xC7);
	io_outb(port+4,0x0B);	//?????
	
	dbgbreak();
	
}


void kdb_break(qword id,qword errcode){
	
	
	
	
	
}

#endif



extern "C"
void dispatch_exception(qword id,qword errcode){

#ifdef _DEBUG
	kdb_break(id,errcode);
#endif
	//TODO handle exceptions here

}



