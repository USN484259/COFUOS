#pragma once
#include "types.hpp"
#include "bugcheck.hpp"

extern "C"{
#ifndef _TEST
	void buildIDT(void);
	qword DR_match(void);
	void DR_get(void*);
	void DR_set(const void*);

	byte serial_get(word);
	void serial_put(word,byte);
	byte serial_peek(word);
#endif

	//void handler_push(exception_handler,void*);
	//void handler_pop(void);
	
	/*
	struct _EXCEPTION_RECORD;
	struct _CONTEXT;
	struct _DISPATCHER_CONTEXT;

	int __C_specific_handler(
		struct _EXCEPTION_RECORD*   ExceptionRecord,
		void*                       EstablisherFrame,
		struct _CONTEXT*            ContextRecord,
		struct _DISPATCHER_CONTEXT* DispatcherContext
	);
	*/

//#pragma warning(disable:4391)
	
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

	qword __readcr3(void);
	void __writecr3(qword);
	//void __writedr(unsigned, qword);
	
	qword __readmsr(dword);
	void __writemsr(dword,qword);
	
	qword __readeflags(void);
	void _disable(void);
	void _enable(void);
	
	qword __rdtsc(void);
	void _WriteBarrier(void);
	
	void __nop(void);
	void __halt(void);
	void __invlpg(void*);
	void __debugbreak(void);
	
	char _InterlockedCompareExchange8(char volatile *,char,char);
	short _InterlockedCompareExchange16(short volatile *,short,short);
	long _InterlockedCompareExchange(long volatile *,long,long);
	__int64 _InterlockedCompareExchange64(__int64 volatile *,__int64,__int64);
	void* _InterlockedCompareExchangePointer(void* volatile *,void*,void*);

	char _InterlockedExchange8(char volatile *,char);
	short _InterlockedExchange16(short volatile *,short);
	long _InterlockedExchange(long volatile *,long);
	__int64 _InterlockedExchange64(__int64 volatile *,__int64);
	void* _InterlockedExchangePointer(void* volatile *,void*);
	
	short _InterlockedIncrement16(short volatile *);
	long _InterlockedIncrement(long volatile*);
	__int64 _InterlockedIncrement64(__int64 volatile *);
	
	short _InterlockedDecrement16(short volatile *);
	long _InterlockedDecrement(long volatile *);
	__int64 _InterlockedDecrement64(__int64 volatile *);
	
	void _mm_pause(void);
	void __debugbreak(void);

	int _rdseed64_step(unsigned __int64 *);
	int _rdrand64_step(unsigned __int64 *);
//#pragma warning(default:4391)
	
}

//#define cmpxchgb(a,b,c) (byte)_InterlockedCompareExchange8((volatile char*)a,(char)b,(char)c)
//#define cmpxchgw(a,b,c) (word)_InterlockedCompareExchange16((volatile short*)a,(short)b,(short)c)
//#define cmpxchgd(a,b,c) (dword)_InterlockedCompareExchange((volatile long*)a,(long)b,(long)c)
//#define cmpxchgq(a,b,c) (qword)_InterlockedCompareExchange64((volatile __int64*)a,(__int64)b,(__int64)c)

//#define xchgb(a,b) (byte)_InterlockedExchange8((volatile char*)a,(char)b)
//#define xchgw(a,b) (word)_InterlockedExchange16((volatile short*)a,(short)b)
//#define xchgd(a,b) (dword)_InterlockedExchange((volatile long*)a,(long)b)
//#define xchgq(a,b) (qword)_InterlockedExchange64((volatile __int64*)a,(__int64)b)

//#define lockincw(a) (word)_InterlockedIncrement16((volatile short*)a)
//#define lockincd(a) (dword)_InterlockedIncrement((volatile long*)a)
//#define lockincq(a) (qword)_InterlockedIncrement64((volatile __int64*)a)

//#define lockdecw(a) (word)_InterlockedDecrement16((volatile short*)a)
//#define lockdecd(a) (dword)_InterlockedDecrement((volatile long*)a)
//#define lockdecq(a) (qword)_InterlockedDecrement64((volatile __int64*)a)
