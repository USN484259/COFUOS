#pragma once
#include "types.h"

namespace UOS{
	struct TSS{
		dword reserved_0;
		qword rsp0;
		qword rsp1;
		qword rsp2;
		qword reserved_1;
		qword ist1;
		qword ist2;
		qword ist3;
		qword ist4;
		qword ist5;
		qword ist6;
		qword ist7;
		qword reserved_2;
		dword reserved_3;
	} __attribute__((packed));
	static_assert(sizeof(TSS) == 104,"TSS size mismatch");

	void build_IDT(void);
	void build_TSS(TSS* tss,qword stk_normal,qword stk_pf,qword stk_fatal);
}

extern "C"{
	void ISR_exception(void);
	void ISR_irq(void);
	void service_entry(void);
	[[ noreturn ]]
	void service_exit(qword rip,qword module_base,qword command,qword rsp);
}