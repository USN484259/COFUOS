#include "apic.hpp"
#include "constant.hpp"
#include "memory/include/vm.hpp"
#include "exception/include/kdb.hpp"
#include "port_io.hpp"
#include "dev/include/acpi.hpp"
#include "sync/include/lock_guard.hpp"
#include "assert.hpp"

using namespace UOS;

static auto apic_base = (dword volatile* const)LOCAL_APIC_VBASE;
static auto io_apic_index = (dword volatile* const)IO_APIC_VBASE;
static auto io_apic_data = (dword volatile* const)(IO_APIC_VBASE + 0x10);

void io_apic_write(word offset,qword value){
	assert(0 == (offset & 1));
	*io_apic_index = offset + 1;
	*io_apic_data = (dword)(value >> 32);

	*io_apic_index = offset;
	*io_apic_data = (dword)value;
}

dword io_apic_read(word offset){
	*io_apic_index = offset;
	return *io_apic_data;
}

void apic_write(word offset,dword value){
	assert(0 == (offset & 3));
	*(apic_base + offset/4) = value;
}

dword apic_read(word offset){
	assert(0 == (offset & 3));
	return *(apic_base + offset/4);
}

APIC::APIC(void) : table{0}{
	qword stat = __readmsr(IA32_APIC_BASE);
	qword base = stat & (~PAGE_MASK);
	if (stat & 0x400){
		//x2APIC, reset
		stat &= (~0xC00);
		__writemsr(IA32_APIC_BASE,stat);
	}
	__writemsr(IA32_APIC_BASE,stat | 0x800);

	//maps local APIC
	auto madt = acpi.get_madt();
	if (madt.local_apic_pbase != base){
		BugCheck(hardware_fault,madt.local_apic_pbase,base);
	}
	auto res = vm.assign(LOCAL_APIC_VBASE,base,1);
	if (!res)
		BugCheck(corrupted,base);

	auto svr = apic_read(0xF0);
	apic_write(0xF0,svr | 0x100);    //APIC enable

	res = false;
	byte apic_id = id();
	byte uid;
	for (auto& p : madt.processors){
		if (p.apic_id == apic_id){
			uid = p.uid;
			res = true;
			break;
		}
	}
	if (!res)
		BugCheck(hardware_fault,apic_id);
	dbgprint("CPU#%d APICid = %d %s",uid,apic_id,(stat & 0x100) ? "BSP" : "AP");
	
	//local APIC setup
	
	apic_write(0x2F0,0x00010000); //CMCI
	apic_write(0x320,(dword)IRQ_APIC_TIMER);  //timer
	apic_write(0x330,0x00010000);    //Thermal Monitor
	apic_write(0x340,0x00010000);    //Perf Counter
	apic_write(0x370,0x00010000);    //Error

	//NMI info
	dword lint[2] = {0x00010000,0x00010000};

	for (auto& p : madt.nmi_pins){
		if (p.uid == 0xFF || p.uid == uid){
			//	15 : LEVEL
			//	13 : LOW_ACTIVE
			dword new_value = 0x2400;	//NMI EDGE
			if (1 == (p.mode & 0x03))
				new_value &= ~(qword)0x2000;
			if (0x0C == (p.mode & 0x0C))
				new_value |= 0x8000;
			if (p.pin > 1)
				BugCheck(hardware_fault,p.pin);
			lint[p.pin] = new_value;
		}
	}
	apic_write(0x350,lint[0]);    //LINT0
	apic_write(0x360,lint[1]);    //LINT1

	if (0 == (stat & 0x100)){	//not BSP
		BugCheck(not_implemented);
		return;
	}
	//disable 8259
	//ports : 20 21 A0 A1
	port_write(0x20,(byte)0x11);
	port_write(0xA0,(byte)0x11);
	port_write(0x21,(byte)0x20);
	port_write(0xA1,(byte)0x28);
	port_write(0x21,(byte)0x04);
	port_write(0xA1,(byte)0x02);
	port_write(0x21,(byte)0x01);
	port_write(0xA1,(byte)0x01);
	//mask all
	port_write(0x21,(byte)0xFF);
	port_write(0xA1,(byte)0xFF);

	//IO APIC init
	/*
	dword rcba_pci = pci_address(0,31,0,0xF0);
	dword rcba_pbase = pci_read(rcba_pci) & 0xFFFFC000;	//bit 31..14
	//OIC @ rcba_view + 0x31FE
	VM::map_view rcba_view(rcba_pbase + 3*PAGE_SIZE);

	word oic = *((word volatile*)rcba_view + 0x1FE/2);
	*((word volatile*)rcba_view + 0x1FE/2) = oic | 0x100;	//enable IO APIC

	dword io_apic_pbase = (dword)0xFEC00000 | (oic & 0xFF) << 12;
	*/
	if (!madt.io_apic_pbase || (madt.io_apic_pbase & PAGE_MASK))
		BugCheck(hardware_fault,madt.io_apic_pbase);
	vm.assign(IO_APIC_VBASE,madt.io_apic_pbase,1);

	//Redirection table
	auto entries = (byte)(1 + (io_apic_read(1) >> 16));
	dbgprint("IO APIC entries = %d",entries);
	vector<qword> rte(entries,(qword)0x00010000);
	qword destination = (qword)apic_id << 56;

	for (unsigned i = 0;i < 15;++i){
		if (i >= madt.gsi_base)
			rte[i - madt.gsi_base] = destination | (IRQ_OFFSET + i);
	}
	auto& fadt = acpi.get_fadt();
	byte sci_irq = (byte)fadt.SCI_Interrupt;
	if (madt.pic_present){
		if (sci_irq < madt.gsi_base)
			BugCheck(hardware_fault,sci_irq);
		sci_irq -= (byte)madt.gsi_base;
	}

	//OSPM is required to treat the ACPI SCI interrupt as a sharable, level, active low interrupt.
	rte[sci_irq] = destination | 0xA000 | IRQ_SCI;

	//IRQ redirect
	for (auto& redirect : madt.redirects){
		if (redirect.gsi > entries)
			BugCheck(hardware_fault,redirect);
		if (redirect.irq >= 16)
			BugCheck(hardware_fault,redirect);
		qword new_value = destination | 0x2000;
		if (redirect.irq == 2)	//NMI
			new_value |= 0x400;
		else{
			new_value |= (IRQ_OFFSET + redirect.irq);
			if (redirect.irq >= madt.gsi_base \
				&& (rte[redirect.irq - madt.gsi_base] & 0xFF) == (IRQ_OFFSET + redirect.irq) )
			{	//clear origin IRQ vector if exists
				rte[redirect.irq - madt.gsi_base] = 0x00010000;
			}
		}
		if (1 == (redirect.mode & 0x03))	//high active
			new_value &= ~(qword)0x2000;
		if (0x0C == (redirect.mode & 0x0C))	//level triggered
			new_value |= 0x8000;
		
		rte[redirect.gsi] = new_value;
	}
	//set IO APIC
	for (unsigned i = 0;i < entries;++i){
		dbgprint("Entry#%d = %x",i,rte[i]);
		io_apic_write((word)(0x10 + 2*i),rte[i]);
	}
	//set IF
	_enable();
}

byte APIC::id(void){
	return (byte)(apic_read(0x20) >> 24);
}

void APIC::dispatch(byte off_id){
	IF_assert;
	assert(off_id < IRQ_MAX - IRQ_MIN);
	irq_handler entry;
	{
		interrupt_guard<spin_lock> guard(lock);
		entry.callback = table[off_id].callback;
		entry.data = table[off_id].data;
	}
	if (entry.callback)
		entry.callback(IRQ_MIN + off_id,entry.data);
}

APIC::CALLBACK APIC::get(byte irq) const{
	if (irq >= IRQ_MIN && irq < IRQ_MAX){
		return table[irq - IRQ_MIN].callback;
	}
}

void APIC::set(byte irq,CALLBACK callback,void* data){
	if (irq >= IRQ_MIN && irq < IRQ_MAX){
		interrupt_guard<spin_lock> guard(lock);
		table[irq - IRQ_MIN] = {callback,data};
		return;
	}
	BugCheck(out_of_range,irq);
}

extern "C"
void dispatch_irq(byte off_id){
	if (off_id < APIC::IRQ_MAX - APIC::IRQ_MIN){
		apic.dispatch(off_id);
	};
	//EOI
	apic_write(0xB0,0);
}