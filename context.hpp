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

#pragma warning(push)
#pragma warning(disable: 4201)
	struct DR_STATE{
		union {
			struct {
				qword dr0;
				qword dr1;
				qword dr2;
				qword dr3;
			};
			qword dr[4];
		};
		qword dr6;
		qword dr7;
	};
#pragma warning(pop)

}