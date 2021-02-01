#pragma once
#include "types.hpp"

namespace UOS{
	struct context {
		qword rax;
		qword rcx;
		qword rdx;
		qword rbx;
		qword rsi;
		qword rdi;
		qword rbp;
		qword r8;
		qword r9;
		qword r10;
		qword r11;
		qword r12;
		qword r13;
		qword r14;
		qword r15;

		qword rip;
		qword cs;
		qword rflags = 0;	//nonzero -> has valid context
		qword rsp;
		qword ss;
	};
	static_assert(sizeof(context) == 8*(5+15),"context_size_mismatch");
	struct SSE_context{
		qword data[0x200/sizeof(qword)];
	};

}