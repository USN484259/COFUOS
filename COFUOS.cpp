
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
	
	PE64 pe(module_base);
	
	
	//globals construct
	fun* globalConstructor = (fun*)
		VM::VMG::construct(pe);	//returns CRT base			//pe.section(strCRT);//(fun*)peGetSection(module_base,strCRT);

	assertinv(nullptr,globalConstructor);
	while(*globalConstructor)
		(*globalConstructor++)();
	
	//give sysheap a block
	{
		void* p = VM::sys->reserve(nullptr,0x200);	//get 2M VM area
		assertinv(nullptr,p);
		VM::sys->commit(p,0x200,PAGE_WRITE | PAGE_NX);	//have somewhere mapped
		bool res=syspool.expand(p,0x200*PAGE_SIZE);		//give it to sys heap
		assert(true,res);
	}
	
	//TODO: place test code for heap here


	apic = new((byte*)APIC_PBASE) APIC;
	*(byte*)(sysinfo+1)=apic->id();
	
	//MP setup
	/*
	sysinfo->krnlbase=(qword)module_base;
	sysinfo->AP_entry=(dword)( (qword)AP_entry-(qword)module_base );
	sysinfo->MP_cnt=1;
	
	mp=new((byte*)MPAREA_BASE) MP();
	*/

}

__declspec(noreturn)
void AP_entry(word pid){
	
	
	buildIDT(pid);
	apic=new((byte*)APIC_PBASE) APIC;
	
	*((byte*)(sysinfo+1)+pid) = apic->id();
	assert(pid,sysinfo->MP_cnt);
	sysinfo->MP_cnt++;
	
	
}