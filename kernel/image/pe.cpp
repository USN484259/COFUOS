#include "pe.hpp"
#include "util.hpp"
#include "assert.hpp"

using namespace UOS;


PE64::PE64(const void* base) : pe_info( (const _info*) ((const byte*)base + *(dword*)((const byte*)base + 0x3C) ) ) , pe_section((const _section*)(pe_info+1)){
	do{
		if (*(const word*)base != 0x5A4D)	//'MZ'
			break;
				
		if (0x4550 != pe_info->signature)	//'PE\0\0'
			break;
			
		if (!pe_info->section_count)
			break;
			
		if (pe_info->size_optheader != 0xF0)
			break;
		
		if (pe_info->imgbase != (qword)base)
			break;
		
		if (pe_info->dir_count != 0x10)
			break;
		
		return;
	}while(false);
	
	BugCheck(invalid_argument,base);
	
}

const void* PE64::base(void) const{
	return (const void*)pe_info->imgbase;
}

dword PE64::header_size(void) const{
	return pe_info->header_size;
}

PE64::section_iterator PE64::section(void) const{
	return section_iterator(*this);
}

pair<dword,dword> PE64::stack(void) const{
	return pair<dword,dword>((dword)pe_info->stk_reserve,(dword)pe_info->stk_commit);
}

PE64::section_iterator::section_iterator(const PE64& p) : pe(p),it(0){}

bool PE64::section_iterator::next(void){
	return (++it < pe.pe_info->section_count);
}

const PE64::_section* PE64::section_iterator::entrance(void) const{
	assert(it < pe.pe_info->section_count);
	return pe.pe_section+it;
}

const char* PE64::section_iterator::name(void) const{
	return entrance()->name;
}

qword PE64::section_iterator::base(void) const{
	return pe.pe_info->imgbase + entrance()->offset;
}

dword PE64::section_iterator::size(void) const{
	return entrance()->size;
}

/*
void* UOS::peGetSection(const void* imgBase,const char* name){
	const byte* p=static_cast<const byte*>(imgBase);
	do{
		if (p[0]=='M' && p[1]=='Z')
			;
		else
			break;
		p+=*(dword*)(p+0x3C);
		
		if (0x4550!=*(dword*)p)	//'PE'
			break;
		
		unsigned cnt= *(word*)(p+0x06);
		
		p+=0x18+*(word*)(p+0x14);
		
		//p+=0x74;
		//p+=*(dword*)p * 8;
		
		while(cnt--){
			if (8==match((const char*)p,name,8))
				return (byte*)imgBase+*(dword*)(p+0x0C);
			
			
			
			p+=0x28;
		}
		
		
		
	}while(false);
	
	return nullptr;
	
}
*/
