
#include "hal.hpp"
#include "sysinfo.hpp"
#include "pe.hpp"
#include "mm.hpp"
#include "list.hpp"
#include "heap.hpp"
#include "apic.hpp"

using namespace UOS;

__declspec(noreturn)
void AP_entry(void);


//qword apic_vbase=0xFFFF800000010000;

SYSINFO* sysinfo=(SYSINFO*)HIGHADDR(0x0800);
heap syspool;
VMG* vmg=(VMG*)HIGHADDR(0x5000);


__declspec(noreturn)
void krnlentry(void* module_base){
	
	sysinfo->MP_lock=0;
	sysinfo->MP_cnt=1;
	sysinfo->AP_entry=(qword)AP_entry-(qword)module_base;
	sysinfo->krnlbase=(qword)module_base;
	
	buildIDT();

#ifdef _DEBUG
	void kdb_init(word);
	kdb_init(sysinfo->ports[0]);
#endif

	APIC apic((byte*)HIGHADDR(0xFEE00000));

	char strCRT[8]={'.','C','R','T',0};
	fun* globalConstructor = (fun*)peGetSection(module_base,strCRT);
	assertinv(nullptr,globalConstructor);
	while(*globalConstructor)
		(*globalConstructor++)();
	

	list<qword> lst;
	auto it=lst.begin();
	lst.push_back(lst.size());
	
}

__declspec(noreturn)
void AP_entry(void){
	
	APIC apic((byte*)HIGHADDR(0xFEE00000));
	
	
	
	
}