#include "acpi.hpp"
#include "util.hpp"
#include "lang.hpp"
#include "bugcheck.hpp"
#include "assert.hpp"
#include "../memory/include/vm.hpp"

using namespace UOS;

#ifdef ACPI_TEST
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE
#endif

#define HEADER_SIZE 36

NOINLINE
bool ACPI::validate(const void* base_addr,size_t limit){
	if (limit < 8)
		return false;
	auto addr = (const byte*)base_addr;
	auto size = *(const dword*)(addr + 4);
	if (size < HEADER_SIZE || size > limit)
		return false;
	byte sum = 0;
	while(size--){
		sum += *addr++;
	}
	return sum == 0;
}


ACPI::ACPI(void) {
	struct RSDP{
		qword addr : 56;
		qword type : 8;
	};
	static_assert(sizeof(RSDP) == 8,"RSDP size mismatch");
	auto rsdp = (RSDP const*)&sysinfo->ACPI_RSDT;
	do{
		if (!rsdp)
			break;
		auto aligned_addr = align_down(rsdp->addr,PAGE_SIZE);
		auto offset = rsdp->addr - aligned_addr;
		VM::map_view rsdt_view(aligned_addr);
		auto view = (dword const*)((byte const*)rsdt_view + offset);
		if (!validate(view,PAGE_SIZE - offset))
			break;
		
		auto size = view[1];
		assert(size >= HEADER_SIZE);
		size -= HEADER_SIZE;
		if (rsdp->type){    //XSDT
			if (*view != 0x54445358 /*XSDT*/ || (size & 0x07))
				break;
			auto it = (qword const*)(view + HEADER_SIZE/4);
			while(size){
				parse_table(*it);
				++it;
				size -= 8;
			}
		}
		else{   //RSDT
			if(*view != 0x54445352 /*RSDT*/ || (size & 0x03))
				break;
			view += (HEADER_SIZE/4);
			while(size){
				parse_table(*view);
				++view;
				size -= 4;
			}
		}
		return;
	}while(false);
	BugCheck(hardware_fault,rsdp->addr);
}

NOINLINE
void ACPI::parse_table(qword pbase){
	auto aligned_pbase = align_down(pbase,PAGE_SIZE);
	auto offset = pbase - aligned_pbase;
	VM::map_view table_view(aligned_pbase);
	auto view = (dword const*)((byte const*)table_view + offset);
	if (!validate(view,PAGE_SIZE - offset))
		return;
	if (*view == 0x43495041 /*APIC*/){
		madt = new MADT(view);
	}
	if (*view == 0x50434146 /*FACP*/){
		if (fadt)
			BugCheck(hardware_fault,pbase);
		fadt = new FADT;
		auto size = *(view + 1);
		memcpy(fadt,view + HEADER_SIZE,min<size_t>(size,sizeof(FADT)));
		if (size < sizeof(FADT))
			zeromemory((byte*)fadt + size,sizeof(FADT) - size);
	}
}

NOINLINE
MADT::MADT(void const* vbase){
	byte const* cur = (byte const*)vbase + HEADER_SIZE;
	auto limit = cur + *((dword const*)vbase + 1);
	local_apic_pbase = *(dword const*)cur;
	if (0 == (*(cur+4) & 0x01)){	//no 8259
		pic_present = false;
	}
	cur += 8;
	bool io_apic_found = false;
	while (cur < limit){
		switch(*cur){
		case 0: //Processor Local APIC
			if (8 == *(cur+1) && (1 & *(cur+4))){
				byte uid = *(cur+2);
				byte apic_id = *(cur+3);
				processors.push_back(processor{uid,apic_id});
			}
			break;
		case 1:	//IO APIC
			if (12 == *(cur+1)){
				auto cur_gsi = *(dword const*)(cur+8);
				if (!io_apic_found || cur_gsi < gsi_base){
					io_apic_pbase = *(dword const*)(cur+4);
					gsi_base = cur_gsi;
				}
				io_apic_found = true;
			}
			break;
		case 2:	//Interrupt Source Override
			if (10 == *(cur+1) && 0 == *(cur+2)){
				dword gsi = *(dword const*)(cur+4);
				byte irq = *(cur+3);
				byte mode = *(cur+8);
				redirects.push_back(redirect{gsi,irq,mode});
			}
			break;
		case 3:	//NMI Source
			if (8 == *(cur+1)){
				dword gsi = *(dword const*)(cur+4);
				byte mode = 0x80 | *(cur+2);	//NMI flag
				redirects.push_back(redirect{gsi,2,mode});
			}
			break;
		case 4:	//Local APIC NMI
			if (6 == *(cur+1)){
				byte uid = *(cur+2);
				byte mode = *(cur+3);
				byte pin = *(cur+5);
				nmi_pins.push_back(nmi{uid,pin,mode});
			}
			break;
		case 5:	//Local APIC Address Override
			if (12 == *(cur+1)){
				local_apic_pbase = *(qword const*)(cur+4);
			}
			break;
		}
		cur += *(cur + 1);
	}
}

const MADT& ACPI::get_madt(void) const{
	if (!madt)
		BugCheck(hardware_fault,this);
	return *madt;
}

const FADT& ACPI::get_fadt(void) const{
	if (!fadt)
		BugCheck(hardware_fault,this);
	return *fadt;
}