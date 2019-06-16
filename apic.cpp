#include "apic.hpp"
#include "hal.hpp"
#include "assert.hpp"

using namespace UOS;

#define IDR 0x020
#define ICR 0x300

APIC::APIC(void){
	page_assert(this);
	qword stat=__readmsr(IA32_APIC_BASE);
	if (stat & 0x400){
		__writemsr(IA32_APIC_BASE,(stat & 0xFFFFF000) );
		__nop();
		_WriteBarrier();
	}
	__writemsr(IA32_APIC_BASE , (stat & 0xFFFFF000) | 0x0800);
	
	
	
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
	
	
	*(dword*)((byte*)this+0xF0) |= 0x100;	//enable
	
}

byte APIC::id(void){
	if (nullptr == this)
		return 0;
	return read(IDR) >> 24;

}

dword APIC::read(size_t off){
	return *(dword*)((byte*)this+off);
}


void APIC::write(size_t off,dword val){
	*(dword*)((byte*)this+off)=val;
}

void APIC::mp_break(void){
	//TODO send IPI here
	if (nullptr==this)
		return;
	
}