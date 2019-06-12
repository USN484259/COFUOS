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
	void* memcpy(void*,const void*,size_t);

	void buildIDT(qword);
	
	
	byte serial_get(word);
	void serial_put(word,byte);
	byte serial_peek(word);

	__declspec(noreturn)
	int BugCheck(UOS::status,qword,qword);


#pragma warning(disable:4391)
	
	byte __inbyte(word);
	word __inword(word);
	dword __indword(word);
	
	void __outbyte(word,byte);
	void __outword(word,word);
	void __outdword(word,dword);
	
	void __stosb(byte*, byte, size_t);
	void __stosw(word*, word, size_t);
	void __stosd(dword*, dword, size_t);
	void __stosq(qword*, qword, size_t);

	void __movsb(byte*, const byte*, size_t);
	void __movsw(word*, const word*, size_t);
	void __movsd(dword*, const dword*, size_t);
	void __movsq(qword*, const qword*, size_t);

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
