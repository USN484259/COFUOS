#include "pe.hpp"
#include "util.hpp"
#include "assert.hpp"

using namespace UOS;

PE64::directory const* PE64::get_directory(unsigned index) const{
	if (index < dir_count)
		return (directory const*)(this + 1) + index;
	return nullptr;
}

PE64::section const* PE64::get_section(unsigned index) const{
	if (index < section_count)
		return (section const*)((directory const*)(this + 1) + dir_count) + index;
	return nullptr;
}

PE64 const* PE64::construct(void const* module_base){
	do{
		if (*(word const*)module_base != 0x5A4D)	//'MZ'
			break;
		auto pe_offset = *((dword const*)module_base + 0x3C/4);
		auto pe = (PE64 const*)((byte const*)module_base + pe_offset);
		if (pe->signature != 0x4550)	//'PE\0\0'
			break;
		if (pe->size_optheader != 0xF0 || pe->machine != 0x8664 || pe->magic != 0x20B)
			break;
		return pe;
	}while(false);
	return nullptr;
}