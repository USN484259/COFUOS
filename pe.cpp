#include "pe.hpp"
#include "util.hpp"

using namespace UOS;


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