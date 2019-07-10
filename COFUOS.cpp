
#include "hal.hpp"
#include "constant.hpp"
#include "sysinfo.hpp"
#include "pe.hpp"
#include "mm.hpp"
#include "list.hpp"
#include "heap.hpp"
#include "apic.hpp"
#include "mp.hpp"

using namespace UOS;



__declspec(noreturn)
void AP_entry(word);


//qword apic_vbase=0xFFFF800000010000;

//VMG* vmg=(VMG*)HIGHADDR(0x5000);


__declspec(noreturn)
void krnlentry(void* module_base){

	buildIDT(0);
	
#ifdef _DEBUG
	void kdb_init(word);
	kdb_init(sysinfo->ports[0]);
#endif
	
	VM::VMG::construct();
	
	//globals construct
	const char strCRT[8]={'.','C','R','T',0};
	fun* globalConstructor = (fun*)peGetSection(module_base,strCRT);
	assertinv(nullptr,globalConstructor);
	while(*globalConstructor)
		(*globalConstructor++)();

	sysinfo->krnlbase=(qword)module_base;
	sysinfo->AP_entry=(qword)AP_entry-(qword)module_base;
	sysinfo->MP_cnt=1;
	
	mp=new((byte*)MPAREA_BASE) MP();
	


	//MP setup
	apic = new((byte*)APIC_PBASE) APIC;
	*(byte*)(sysinfo+1)=apic->id();
	
	


}

__declspec(noreturn)
void AP_entry(word pid){
	
	
	buildIDT(pid);
	apic=new((byte*)APIC_PBASE) APIC;
	
	*((byte*)(sysinfo+1)+pid) = apic->id();
	assert(pid,sysinfo->MP_cnt);
	sysinfo->MP_cnt++;
	
	
}