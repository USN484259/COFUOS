#pragma once
#include "types.hpp"

extern "C"{
	void build_IDT(void);
	void fpu_init(void);
	void service_entry(void);
	[[ noreturn ]]
	void service_exit(qword rip,qword module_base,qword command,qword rsp);
}