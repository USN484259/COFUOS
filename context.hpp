#pragma once
#include "types.hpp"

namespace UOS {

	struct CONTEXT {
		qword r15;
		qword r14;
		qword r13;
		qword r12;
		qword r11;
		qword r10;
		qword r9;
		qword r8;
		qword rdi;
		qword rsi;
		qword rbx;
		qword rdx;
		qword rcx;
		qword rax;
		qword rbp;
		qword reserved;
		qword errcode;
		qword rip;
		qword cs;
		qword rflags;
		qword rsp;
		qword ss;


	};

}