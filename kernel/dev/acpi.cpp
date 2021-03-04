#include "acpi.hpp"
#include "util.hpp"
#include "lang.hpp"
#include "assert.hpp"
#include "memory/include/vm.hpp"
#include "intrinsics.hpp"

using namespace UOS;

#define HEADER_SIZE 36

void UOS::shutdown(void){
	while (true){
		out_word(0xB004, 0x2000);	//Bochs shutdown
		out_word(0x4004, 0x3400);	//VirtualBox shutdown
		halt();
	}
}

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
	auto& rsdp = sysinfo->rsdp;
	do{
		if (!rsdp.address)
			break;
		dbgprint("Going through ACPI");
		auto aligned_addr = align_down(rsdp.address,PAGE_SIZE);
		auto offset = rsdp.address - aligned_addr;
		map_view rsdt_view(aligned_addr);
		auto view = (dword const*)((byte const*)rsdt_view + offset);
		if (!validate(view,PAGE_SIZE - offset))
			break;
		
		auto size = view[1];
		assert(size >= HEADER_SIZE);
		size -= HEADER_SIZE;
		version = rsdp.version;
		if (rsdp.version){    //XSDT
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
	bugcheck("invalid RSDP @ %p",rsdp.address);
}


void ACPI::parse_table(qword pbase){
	auto aligned_pbase = align_down(pbase,PAGE_SIZE);
	auto offset = pbase - aligned_pbase;
	map_view table_view(aligned_pbase);
	auto view = (dword const*)((byte const*)table_view + offset);
	if (!validate(view,PAGE_SIZE - offset))
		return;
	char table_name[5];
	memcpy(table_name,view,4);
	table_name[4] = 0;
	dbgprint("ACPI table %s @ %x ~ %x",table_name,pbase,pbase + *(view + 1));
	if (*view == 0x43495041 /*APIC*/){
		madt = new MADT(view);
		return;
	}
	if (*view == 0x50434146 /*FACP*/){
		if (fadt)
			bugcheck("duplicated FADT @ %p",pbase);
		fadt = new FADT(view);
		return;
	}
	if (*view == 0x54455048 /*HPET*/){
		hpet = new HPET(view);
		return;
	}
}

byte ACPI::get_version(void) const{
	return version;
}

const MADT& ACPI::get_madt(void) const{
	if (!madt)
		bugcheck("MADT not present");
	return *madt;
}

const FADT& ACPI::get_fadt(void) const{
	if (!fadt)
		bugcheck("FADT not present");
	return *fadt;
}

const HPET& ACPI::get_hpet(void) const{
	if (!hpet)
		bugcheck("HPET not present");
	return *hpet;
}

FADT::FADT(const dword* view){
	auto size = *(view + 1);
	memcpy(this,view + HEADER_SIZE/4,min<size_t>(size,sizeof(FADT)));
	if (size < sizeof(FADT)){
		zeromemory((byte*)this + size,sizeof(FADT) - size);
	}
	dbgprint("Profile : %d",PreferredPowerManagementProfile);
	dbgprint("SCI IRQ %d",SCI_Interrupt);
	dbgprint("RTC century support : %s",Century ? "true" : "false");
	dbgprint("FADT flags = 0x%x",(qword)Flags);
	dbgprint("FADT BootArch = 0x%x",(qword)BootArchitectureFlags);
}

HPET::HPET(const dword* view){
	auto size = *(view + 1);
	memcpy(this,view + HEADER_SIZE/4,min<size_t>(size,sizeof(HPET)));

	assert(address.AddressSpace == 0);
	dbgprint("HPET @ %p %s-bit with %d comparators",\
		address.Address, counter_size ? "64" : "32", comparator_count + 1);
	
}

MADT::MADT(void const* vbase){
	byte const* cur = (byte const*)vbase;
	auto limit = cur + *((dword const*)vbase + 1);
	cur += HEADER_SIZE;
	local_apic_pbase = *(dword const*)cur;
	if (0 == (*(cur+4) & 0x01)){	//no 8259
		pic_present = false;
	}
	dbgprint("8259 PIC present : %s",pic_present ? "true" : "false");
	cur += 8;
	bool io_apic_found = false;
	while (cur < limit){
		switch(*cur){
		case 0: //Processor Local APIC
			if (8 == *(cur+1) && (1 & *(cur+4))){
				byte uid = *(cur+2);
				byte apic_id = *(cur+3);
				dbgprint("Processor #%d : APICid = 0x%x",uid,(qword)apic_id);
				processors.push_back(processor{uid,apic_id});
			}
			break;
		case 1:	//IO APIC
			if (12 == *(cur+1)){
				auto cur_gsi = *(dword const*)(cur+8);
				dbgprint("IO APIC id = %d GSI#%d @ %p",*(cur+2),cur_gsi,(qword)*(dword const*)(cur+4));
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
				dbgprint("IRQ#%d ==> GSI#%d mode 0x%x",irq,gsi,(qword)mode);
				redirects.push_back(redirect{gsi,irq,mode});
			}
			break;
		case 3:	//NMI Source
			if (8 == *(cur+1)){
				dword gsi = *(dword const*)(cur+4);
				byte mode = *(cur+2);
				dbgprint("GSI#%d as NMI mode 0x%x",gsi,(qword)mode);
				redirects.push_back(redirect{gsi,2,mode});
			}
			break;
		case 4:	//Local APIC NMI
			if (6 == *(cur+1)){
				byte uid = *(cur+2);
				byte mode = *(cur+3);
				byte pin = *(cur+5);
				dbgprint("NMI pin%d for Processor#%d mode 0x%x",pin,uid,(qword)mode);
				nmi_pins.push_back(nmi{uid,pin,mode});
			}
			break;
		case 5:	//Local APIC Address Override
			if (12 == *(cur+1)){
				local_apic_pbase = *(qword const*)(cur+4);
				dbgprint("local APIC redirect @ %p",local_apic_pbase);
			}
			break;
		}
		if (0 == *(cur + 1))
			bugcheck("invalid ACPI table @ %p",cur);
		cur += *(cur + 1);
	}
}

