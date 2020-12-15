#include "cpu.hpp"
#include "hal.hpp"
#include "assert.hpp"
#include "port_io.hpp"

using namespace UOS;

interrupt_guard::interrupt_guard(void) : \
	state(__readeflags() & 0x0200 ? true : false) \
{
	_disable();
}
interrupt_guard::~interrupt_guard(void){
	if (state)
		_enable();
}

dword pci_address(byte bus,byte device,byte function,byte offset){
	assert(bus < 0x100 && device < 0x20 && function < 0x08 && 0 == (offset & 0x03));
	return (dword)0x80000000 \
		| ((dword)bus << 16) \
		| ((dword)device << 11) \
		| ((dword)function << 8) \
		| ((dword)offset);
}

dword pci_read(dword address){
	assert(0 == (address & 0x7F000003));
	port_write(0xCF8,address);
	dword res;
	port_read(0xCFC,res);
	return res;
}

void pci_write(dword address,dword value){
	assert(0 == (address & 0x7F000003));
	port_write(0xCF8,address);
	port_write(0xCFC,value);
}