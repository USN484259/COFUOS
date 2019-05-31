#pragma once
#include "types.hpp"

#define IA32_APIC_BASE 0x1B



extern "C"{
	void buildIDT(void);
	qword IF_get(void);
	byte io_inb(word port);
	word io_inw(word port);
	dword io_ind(word port);
	void io_outb(word port,byte val);
	void io_outw(word port,word val);
	void io_outd(word port,dword val);
	
	qword _rdmsr(dword);
	qword _wrmsr(dword,qword);
	void dbgbreak(void);

}