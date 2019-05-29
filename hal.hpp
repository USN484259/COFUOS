#pragma once
#include "types.hpp"

namespace UOS{
static const dword IA32_APIC_BASE=0x1B;


}

extern "C"{
	void buildIDT();
	qword _rdmsr(dword);
	qword _wrmsr(dword,qword);


}