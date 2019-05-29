
#include "hal.hpp"
#include "pe.hpp"
#include "list.hpp"
#include "heap.hpp"
#include "apic.hpp"

using namespace UOS;


//qword apic_vbase=0xFFFF800000010000;

heap syspool;
APIC apic(reinterpret_cast<byte*>(0xFFFF800000010000));


__declspec(noreturn)
void krnlentry(void* module_base){
	
	char strCRT[8]={'.','C','R','T',0};
	fun* globalConstructor = (fun*)peGetSection(module_base,strCRT);
	assert(globalConstructor);
	while(*globalConstructor)
		(*globalConstructor++)();
	
	buildIDT();
	
	list<qword> lst;
	auto it=lst.begin();
	lst.push_back(lst.size());
	
}