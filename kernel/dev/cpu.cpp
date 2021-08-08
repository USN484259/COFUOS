#include "cpu.hpp"
#include "lang.hpp"
#include "constant.hpp"
#include "intrinsics.hpp"

using namespace UOS;

void UOS::build_IDT(void){
	auto isr_ptr = reinterpret_cast<qword>(ISR_exception);
	struct IDT{
		qword offset0 : 16;
		qword selector : 16;
		qword ist : 3;
		qword : 5;
		qword attrib : 8;
		qword offset1 : 16;
		qword offset2 : 32;
		qword : 0;
	};
	static_assert(sizeof(IDT) == 0x10,"IDT descriptor mismatch");
	auto idt_table = (IDT*)IDT_BASE;
	unsigned index = 0;
	while(index < 20){
		auto& idt = idt_table[index];
		idt.offset0 = isr_ptr;
		idt.selector = SEG_KRNL_CS;
		idt.attrib = 0x8E;
		idt.offset1 = isr_ptr >> 16;
		idt.offset2 = isr_ptr >> 32;
		switch(index){
			case 0x02:	//NMI
			case 0x08:	//DF
			case 0x12:	//MC
				idt.ist = 2;
				break;
			case 0x0E:	//PF
				idt.ist = 1;
				break;
			default:
				idt.ist = 0;
		}
		isr_ptr += 8;
		++index;
	}
	while(index < 0x20){
		idt_table[index++] = {0};
	}
	isr_ptr = reinterpret_cast<qword>(ISR_irq);
	while(index < IDT_LIM/sizeof(IDT)){
		auto& idt = idt_table[index];
		idt.offset0 = isr_ptr;
		idt.selector = SEG_KRNL_CS;
		idt.ist = 0;
		idt.attrib = 0x8E;
		idt.offset1 = isr_ptr >> 16;
		idt.offset2 = isr_ptr >> 32;
		isr_ptr += 8;
		++index;
	}
}

void UOS::build_TSS(TSS* tss,qword stk_normal,qword stk_pf,qword stk_fatal){
	zeromemory(tss,sizeof(TSS));
	tss->rsp0 = stk_normal;	//normal
	tss->ist1 = stk_pf;		//#PF
	tss->ist2 = stk_fatal;	//fatal

	auto tss_addr = reinterpret_cast<qword>(tss);
	struct {
		qword limit : 16;
		qword base0 : 24;
		qword attrib : 16;
		qword base1 : 8;
		qword base2 : 32;
		qword : 0;
	}tss_descriptor = {103,tss_addr,0x0089,tss_addr >> 24,tss_addr >> 32};
	static_assert(sizeof(tss_descriptor) == 0x10,"TSS descriptor mismatch");
	auto gdt = (volatile qword*)GDT_BASE;
	unsigned index = 8;
	while(index < GDT_LIM/8){
		auto val = gdt[index];
		if (val){
			index += 2;
			continue;
		}
		if (val != cmpxchg(gdt + index,*(qword*)&tss_descriptor,val)){
			index += 2;
			continue;
		}
		val = xchg(gdt + index + 1,*((qword*)&tss_descriptor + 1));
		if (val != 0)
			bugcheck("bad GDT entrance (%x) @ %p",val,gdt + index);
		
		ltr(index << 3);
		return;
	}
	bugcheck("no GDT entrance @ %p",gdt);
}

void UOS::delay_us(word cnt){
	while(cnt--){
		__asm__ volatile (
			"out 0x80,al"
		);
	}
}