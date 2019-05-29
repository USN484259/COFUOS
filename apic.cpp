#include "apic.hpp"
#include "hal.hpp"
#include "assert.hpp"

using namespace UOS;



APIC::APIC(byte* vbase) : base(vbase){
	page_assert(vbase);
	qword stat=_rdmsr(IA32_APIC_BASE);
	_wrmsr(IA32_APIC_BASE , (stat & 0xFFFFF000) | 0x0800);
	
	
	//TODO init APIC here (BSP?)
	
	//TODO map to virtualpage
	
	*(dword*)(base+0xF0) |= 0x100;
	
}
/*

APIC::~APIC(void){
	*(dword*)(base+0xF0) &= ~0x100;
}

*/