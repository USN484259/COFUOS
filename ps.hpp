#pragma once
#include "types.hpp"
#include "context.hpp"

namespace UOS{
	
	/*
	class process : public object{
		dword id;
		//word status;
		dword exit_code;
		qword pdpt_low;
		
		list<thread> threads;
		
		set<handle> resources;
		
	public:
		process(void);
		process(const process&)=delete;
		process& operator=(const process&)=delete;
		virtual ~process(void);
		
		dword id(void) const;
		
		
	};
	
	class thread : public object{
		process& ps;
		dword id;
		dword mxcsr;	//SSEx status register
		
		qword rip;
		qword rflags;
		qword rax;
		qword rcx;
		qword rdx;
		qword rbx;
		qword rsp;
		qword rbp;
		qword rsi;
		qword rdi;
		qword r8;
		qword r9;
		qword r10;
		qword r11;
		qword r12;
		qword r13;
		qword r14;
		qword r15;
		
		//XMM registers here
		oword xmm[0x10];

	public:
		thread(process&,qword,size_t);
		virtual ~thread(void);
		
		dword id(void) const;
		
	}
	*/
	
	/*
	namespace PS{
	
		word id(void);
	}
	namespace TH{
	
		dword id(void);
		
	}
	*/
}