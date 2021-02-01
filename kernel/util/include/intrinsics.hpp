#pragma once
#include "types.hpp"

#define ASM __asm__ volatile

namespace UOS{
	inline byte in_byte(word port){
		byte data;
		ASM (
			"in al, dx"
			: "=a" (data)
			: "d" (port)
		);
		return data;
	}
	inline word in_word(word port){
		word data;
		ASM (
			"in ax,dx"
			: "=a" (data)
			: "d" (port)
		);
		return data;
	}
 
	inline void out_byte(word port,byte data){
		ASM (
			"out dx,al"
			:
			: "d" (port), "a" (data)
		);
	}
	inline void out_word(word port,word data){
		ASM (
			"out dx,ax"
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
	inline void halt(void){
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

	inline void swapgs(void){
		ASM (
			"swapgs"
		);
	}
	/*
	inline void int3(void){
		ASM (
			"int 3"
		);
	}
	*/
	template<byte V> 
	inline void int_trap(void){
		ASM (
			"int %0"
			:
			: "i" (V)
		);
	}

	inline void mm_pause(void){
		ASM (
			"pause"
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

	template<typename T>
	inline T read_gs(qword offset){
		T data;
		ASM (
			"mov %0, gs:[%1]"
			: "=r" (data)
			: "ir" (offset)
			: "memory"
		);
		return data;
	}

	template<typename T>
	inline void write_gs(qword offset,T data){
		ASM (
			"mov gs:[%0], %1"
			:
			: "ir" (offset), "ir" (data)
			: "memory"
		);
	}

	template<typename T>
	inline T cmpxchg(T volatile* dst,T xchg,T cmp){
		ASM (
			"cmpxchg %1, %2"
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
	T* cmpxchg_ptr(T* volatile* dst,T* xchg,T* cmp){
		return reinterpret_cast<T*>(
			cmpxchg<qword>(
				reinterpret_cast<qword volatile*>(dst),
				reinterpret_cast<qword>(cmp),
				reinterpret_cast<qword>(xchg)
			)
		);
	}
	template<typename T>
	T* xchg_ptr(T* volatile* dst,T* val){
		return reinterpret_cast<T*>(
			xchg<qword>(
				reinterpret_cast<qword volatile*>(dst),
				reinterpret_cast<qword>(val)
			)
		);
	}
}

#undef ASM