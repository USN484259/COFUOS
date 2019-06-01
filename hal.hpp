#pragma once
#include "types.hpp"

#define IA32_APIC_BASE 0x1B



extern "C"{
	
	void* memset(void*,int,size_t);
	void* zeromemory(void*,size_t);
	
	void buildIDT(void);
	qword IF_get(void);
	byte io_inb(word port);
	word io_inw(word port);
	dword io_ind(word port);
	void io_outb(word port,byte val);
	void io_outw(word port,word val);
	void io_outd(word port,dword val);
	
	qword CR3_get(void);
	qword CR3_set(qword);
	
	qword _rdmsr(dword);
	qword _wrmsr(dword,qword);
	
	void _invlpg(qword,qword);
	
	byte _cmpxchgb(byte*,byte,byte);
	word _cmpxchgw(word*,word,word);
	dword _cmpxchgd(dword*,dword,dword);
	
	void dbgbreak(void);
	
	__declspec(noreturn)
	void BugCheck(UOS::status,qword,qword);

}