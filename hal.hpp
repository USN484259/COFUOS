#pragma once
#include "types.hpp"


#define IA32_APIC_BASE 0x1B

#define PAGE_PRESENT 0x01
#define PAGE_WRITE 0x02
#define PAGE_USER 0x04
#define PAGE_WT 0x08
#define PAGE_CD 0x10


#define PAGE_LARGE 0x80
#define PAGE_GLOBAL 0x100
#define PAGE_COMMIT 0x4000000000000000
#define PAGE_NX 0x8000000000000000


extern "C"{
	
	void* memset(void*,int,size_t);
	void* zeromemory(void*,size_t);

	void buildIDT(qword);
	
	__declspec(noreturn)
	int BugCheck(UOS::status,qword,qword);


#pragma warning(disable:4391)
	
	byte __inbyte(word);
	word __inword(word);
	dword __indword(word);
	
	void __outbyte(word,byte);
	void __outword(word,word);
	void __outdword(word,dword);
	
	qword _readcr3(void);
	void _writecr3(qword);
	
	qword __readmsr(dword);
	void __writemsr(dword,qword);
	
	qword __readeflags(void);
	void _disable(void);
	void _enable(void);
	
	void _WriteBarrier(void);
	
	void __nop(void);
	void _invlpg(void*);
	
	byte _InterlockedCompareExchange8(volatile byte*,byte,byte);
	word _InterlockedCompareExchange16(volatile word*,word,word);
	dword _InterlockedCompareExchange(volatile dword*,dword,dword);
	qword _InterlockedCompareExchange64(volatile qword*,qword,qword);
	
	byte _InterlockedExchange8(volatile byte*,byte);
	word _InterlockedExchange16(volatile word*,word);
	dword _InterlockedExchange(volatile dword*,dword);
	qword _InterlockedExchange64(volatile qword*,qword);
	
	byte _InterlockedIncrement8(volatile byte*);
	word _InterlockedIncrement16(volatile word*);
	dword _InterlockedIncrement(volatile dword*);
	qword _InterlockedIncrement64(volatile qword*);
	
	void _mm_pause(void);
	void __debugbreak(void);
#pragma warning(default:4391)
	
}
/*
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
	
	#pragma message("invlpg in ISR via IPI")
	void _invlpg(qword,qword);
	
	byte _cmpxchgb(volatile byte*,byte,byte);
	word _cmpxchgw(volatile word*,word,word);
	dword _cmpxchgd(volatile dword*,dword,dword);
	
	dword _xchgd(dword* dst,dword val);
	qword _xchgq(qword* dst,qword val);
	
	void dbgbreak(void);
	
	__declspec(noreturn)
	int BugCheck(UOS::status,qword,qword);

}
*/