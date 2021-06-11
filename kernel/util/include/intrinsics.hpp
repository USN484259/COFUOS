#pragma once
#include "types.h"
#include "process/include/context.hpp"

#define ASM __asm__ volatile

namespace UOS{
	inline byte in_byte(word port){
		byte data;
		ASM (
			"in %0,%1"
			: "=a" (data)
			: "d" (port)
		);
		return data;
	}
	inline word in_word(word port){
		word data;
		ASM (
			"in %0,%1"
			: "=a" (data)
			: "d" (port)
		);
		return data;
	}
	inline dword in_dword(word port){
		dword data;
		ASM (
			"in %0,%1"
			: "=a" (data)
			: "d" (port)
		);
		return data;
	}
 
	inline void out_byte(word port,byte data){
		ASM (
			"out %0,%1"
			:
			: "d" (port), "a" (data)
		);
	}
	inline void out_word(word port,word data){
		ASM (
			"out %0,%1"
			:
			: "d" (port), "a" (data)
		);
	}
	inline void out_dword(word port,dword data){
		ASM (
			"out %0,%1"
			:
			: "d" (port), "a" (data)
		);
	}

	inline qword rdmsr(dword id){
		dword lo,hi;
		ASM (
			"rdmsr"
			: "=a" (lo), "=d" (hi)
			: "c" (id)
		);
		return ((qword)hi << 32) | lo;
	}
	inline void wrmsr(dword id,qword data){
		dword lo = data;
		dword hi = data >> 32;
		ASM (
			"wrmsr"
			:
			: "a" (lo), "d" (hi), "c" (id)
		);
	}

	inline dword read_eflags(void){
		qword data;
		ASM (
			"pushfq\n\
			pop %0"
			: "=r" (data)
		);
		return data;
	}
	inline void cli(void){
		ASM (
			"cli"
		);
	}
	inline void sti(void){
		ASM (
			"sti"
		);
	}

	inline qword rdtsc(void){
		dword lo,hi;
		ASM (
			"rdtsc"
			: "=a" (lo), "=d" (hi)
		);
		return ((qword)hi << 32) | lo;
	}
	inline void hlt(void){
		ASM (
			"hlt"
		);
	}
	inline void invlpg(void* ptr){
		ASM (
			"invlpg %0"
			:
			: "m" (*(byte*)ptr)
		);
	}

	inline void clts(void){
		ASM (
			"clts"
		);
	}

	inline void fpu_init(void){
		const dword val = 0x1F80;
		ASM (
			"fninit\n\
			ldmxcsr %0"
			:
			: "m" (val)
		);
	}

	inline void fxsave(SSE_context* ptr){
		ASM (
			"FXSAVE64 %0"
			: "=m" (*ptr)
		);
	}

	inline void fxrstor(const SSE_context* ptr){
		ASM (
			"FXRSTOR64 %0"
			:
			: "m" (*ptr)
		);
	}

	inline void swapgs(void){
		ASM (
			"swapgs"
		);
	}

	inline void int_trap(byte v){
		ASM (
			"int %0"
			:
			: "i" (v)
		);
	}

	inline void mm_pause(void){
		ASM (
			"pause"
		);
	}

	inline qword read_cr0(void){
		qword data;
		ASM (
			"mov %0,cr0"
			: "=r" (data)
		);
		return data;
	}

	inline void write_cr0(qword data){
		ASM (
			"mov cr0,%0"
			:
			: "r" (data)
		);
	}

	inline qword read_cr3(void){
		qword data;
		ASM (
			"mov %0,cr3"
			: "=r" (data)
		);
		return data;
	}

	inline void write_cr3(qword data){
		ASM (
			"mov cr3,%0"
			:
			: "r" (data)
		);
	}

	inline qword read_cr2(void){
		qword data;
		ASM (
			"mov %0,cr2"
			: "=r" (data)
		);
		return data;
	}

	inline void ltr(word sel){
		ASM (
			"ltr %0"
			:
			: "rm" (sel)
		);
	}

	template<typename T>
	inline T read_gs(qword offset){
		T data;
		__asm__ (
			"mov %0, gs:[%1]"
			: "=r" (data)
			: "ir" (offset)
			: "memory"
		);
		return data;
	}

	template<typename T>
	inline void write_gs(qword offset,T data){
		__asm__ (
			"mov gs:[%0], %1"
			:
			: "ir" (offset), "r" (data)
			: "memory"
		);
	}

	template<typename T>
	inline T cmpxchg(T volatile* dst,T xchg,T cmp){
		ASM (
			"lock cmpxchg %1, %2"
			: "+a" (cmp), "+m" (*dst)
			: "r" (xchg)
		);
		return cmp;
	}

	template<typename T>
	inline T xchg(T volatile* dst,T val){
		ASM (
			"xchg %0, %1"
			: "+m" (*dst), "+r" (val)
		);
		return val;
	}

	template<typename T>
	inline T* cmpxchg_ptr(T* volatile* dst,T* xchg,T* cmp){
		return reinterpret_cast<T*>(
			cmpxchg(
				reinterpret_cast<qword volatile*>(dst),
				reinterpret_cast<qword>(xchg),
				reinterpret_cast<qword>(cmp)
			)
		);
	}
	template<typename T>
	inline T* xchg_ptr(T* volatile* dst,T* val){
		return reinterpret_cast<T*>(
			xchg(
				reinterpret_cast<qword volatile*>(dst),
				reinterpret_cast<qword>(val)
			)
		);
	}
	template<typename T>
	inline void lock_add(T volatile* dst,T val){
		ASM (
			"lock add %0, %1"
			: "+m" (*dst)
			: "ri" (val)
		);
	}
	template<typename T>
	inline void lock_sub(T volatile* dst,T val){
		ASM (
			"lock sub %0, %1"
			: "+m" (*dst)
			: "ri" (val)
		);
	}
	template<typename T>
	inline void lock_or(T volatile* dst,T val){
		ASM (
			"lock or %0, %1"
			: "+m" (*dst)
			: "ri" (val)
		);
	}

	inline void* return_address(void){
		return __builtin_return_address(0);
	}
}

#undef ASM