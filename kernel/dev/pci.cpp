#include "pci.hpp"
#include "lang.hpp"
#include "constant.hpp"
#include "assert.hpp"
#include "intrinsics.hpp"

using namespace UOS;

dword PCI::read(word func,byte off){
	IF_assert;
	dword req = ((dword)func << 8) | off | 0x80000000;
	out_dword(PCI_CONFIG_ADDRESS,req);
	return in_dword(PCI_CONFIG_DATA);
}

void PCI::write(word func,byte off,dword val){
	IF_assert;
	dword req = ((dword)func << 8) | off | 0x80000000;
	out_dword(PCI_CONFIG_ADDRESS,req);
	out_dword(PCI_CONFIG_DATA,val);
}

PCI::PCI(void){
	dbgprint("Enumerating PCI devices");
	check_bus(0);
}

void PCI::check_bus(byte bus){
	for (byte dev = 0;dev < 32;++dev){
		if (0xFFFF == (read(pci_function(bus,dev,0),0) & 0xFFFF))
			continue;
		check_function(pci_function(bus,dev,0));
		byte header = read(pci_function(bus,dev,0),0x0C) >> 16;
		if (header & 0x80){
			//multi-function
			for (byte fun = 1;fun < 8;++fun){
				if (0xFFFF == (read(pci_function(bus,dev,fun),0) & 0xFFFF))
					continue;
				check_function(pci_function(bus,dev,fun));
			}
		}
		if (1 == (header & 0x7F)){
			//PCI-to-PCI bridge
			byte secondary_bus = read(pci_function(bus,dev,0),0x18) >> 8;
			check_bus(secondary_bus);
		}
	}
}

void PCI::check_function(word func){
	dword vender = read(func,0);
	dword classify = read(func,0x08);
	word interrupt = read(func,0x3C);
	
	devices.push_back(device_info{func,interrupt,vender,classify});

	byte class_id = classify >> 24;
	byte subclass = classify >> 16;
	byte interface = classify >> 8;
	byte int_pin = interrupt >> 8;
	byte int_line = interrupt;
	
	byte bus = func >> 8;
	byte dev = (func >> 3) & 0x1F;
	byte fun = func & 7;
	dbgprint("PCI function %d,%d,%d : class %x,%x,%x interrupt %d,%d",bus,dev,fun,(qword)class_id,(qword)subclass,(qword)interface,int_pin,int_line);
}

const PCI::device_info* PCI::find(byte class_id,byte subclass,const device_info* ptr) const{
	word cid = ((word)class_id << 8) | subclass;
	if (ptr)
		++ptr;
	else
		ptr = devices.begin();
	while(ptr != devices.end()){
		if ((ptr->classify >> 16) == cid)
			return ptr;
		++ptr;
	}
	return nullptr;
}