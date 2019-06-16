
#include "hal.hpp"
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
	__assume(sysinfo==(SYSINFO*)HIGHADDR(0x0800));
	__assume(mp==(MP*)HIGHADDR(0x1000));
	__assume(apic==(APIC*)HIGHADDR(0xFEE00000));


	buildIDT(0);

	//globals construct
	const char strCRT[8]={'.','C','R','T',0};
	fun* globalConstructor = (fun*)peGetSection(module_base,strCRT);
	assertinv(nullptr,globalConstructor);
	while(*globalConstructor)
		(*globalConstructor++)();

	sysinfo->krnlbase=(qword)module_base;
	sysinfo->AP_entry=(qword)AP_entry-(qword)module_base;
	sysinfo->MP_cnt=1;
	
	mp=new((byte*)HIGHADDR(0x1000)) MP();
	

#ifdef _DEBUG
	void kdb_init(word);
	kdb_init(sysinfo->ports[0]);
#endif

	//MP setup
	apic = new((byte*)HIGHADDR(0xFEE00000)) APIC;
	*(byte*)(sysinfo+1)=apic->id();
	
	


}

__declspec(noreturn)
void AP_entry(word pid){
	__assume(sysinfo==(SYSINFO*)HIGHADDR(0x0800));
	__assume(mp==(MP*)HIGHADDR(0x1000));
	__assume(apic==(APIC*)HIGHADDR(0xFEE00000));
	
	
	buildIDT(pid);
	apic=new((byte*)HIGHADDR(0xFEE00000)) APIC;
	
	*((byte*)(sysinfo+1)+pid) = apic->id();
	assert(pid,sysinfo->MP_cnt);
	sysinfo->MP_cnt++;
	
	
}