#include "apic.hpp"
#include "hal.hpp"
#include "assert.hpp"

using namespace UOS;

const size_t APIC::ICR = 0x300;

APIC::APIC(byte* vbase) : base(vbase){
	page_assert(vbase);
	qword stat=_rdmsr(IA32_APIC_BASE);
	_wrmsr(IA32_APIC_BASE , (stat & 0xFFFFF000) | 0x0800);
	
	
	
	if (stat & 0x0100){		//BSP
		//TODO map to virtualpage
		
		
		//IPI_INIT
		write(ICR+0x10,0);
		write(ICR,0xC4500);	//1100_0100_0101_00000000_b

		
		//delay here
		
		//SIPI two times
		write(ICR,0xC4602);	//1100_0100_0110_00000010_b
		
		//delay here
		write(ICR,0xC4602);	//1100_0100_0110_00000010_b
	
	}
	//TODO init APIC here
	
	
	*(dword*)(base+0xF0) |= 0x100;	//enable
	
}

//#pragma optimize( "", off )
__declspec(noinline)
dword APIC::read(size_t off){
	return *(dword*)(base+off);
}
__declspec(noinline)
void APIC::write(size_t off,dword val){
	*(dword*)(base+off)=val;
}

//#pragma optimize( "", on )
/*

APIC::~APIC(void){
	*(dword*)(base+0xF0) &= ~0x100;
}

*/