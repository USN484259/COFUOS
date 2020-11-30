#include "types.hpp"
#include "assert.hpp"
#include "cpu/include/hal.hpp"
#include "constant.hpp"
#include "sysinfo.hpp"
#include "image/include/pe.hpp"
#include "memory/include/vm.hpp"
#include "memory/include/pm.hpp"
//#include "list.hpp"
#include "memory/include/heap.hpp"
//#include "apic.hpp"
//#include "mp.hpp"
#include "exception/include/kdb.hpp"


using namespace UOS;


[[ noreturn ]]
void AP_entry(word);


//qword apic_vbase=0xFFFF800000010000;

//VMG* vmg=(VMG*)HIGHADDR(0x5000);


[[ noreturn ]]
void krnlentry(void* module_base){
	buildIDT();

#ifdef ENABLE_DEBUGGER
		void kdb_init(word);
	kdb_init(sysinfo->ports[0]);
#endif


	PE64 pe(module_base);
	
	{
		auto it=pe.section();
		procedure* globalConstructor=nullptr;
		const char strCRT[8]={'.','C','R','T',0};

		do{
			if (8==match( strCRT,it.name(),8 ) ){
				globalConstructor=(procedure*)it.base();
				break;
			}
			
		}while(it.next());
		
		assert(nullptr != globalConstructor);
		
		//globals construct
		while(*globalConstructor)
			(*globalConstructor++)();
		
	}
	/*
	VM::VMG::construct(pe);	//returns CRT base			//pe.section(strCRT);//(fun*)peGetSection(module_base,strCRT);

	
	//give sysheap a block
	{
		void* p = VM::sys->reserve(nullptr,0x20);	//get 32K VM area
		assert(nullptr != p);
		VM::sys->commit(p,0x20,PAGE_WRITE | PAGE_NX);	//have somewhere mapped
		bool res=uos_heap->expand(p,0x20*PAGE_SIZE);		//give it to sys heap
		assert(res);
	}
	
	//WARNING : PM::construct needs operator new
	PM::construct((const void*)PMMSCAN_BASE);
	*/

	//	apic = new((byte*)APIC_PBASE) APIC;
	//	*(byte*)(sysinfo+1)=apic->id();
	
	//MP setup
	/*
	sysinfo->krnlbase=(qword)module_base;
	sysinfo->AP_entry=(dword)( (qword)AP_entry-(qword)module_base );
	sysinfo->MP_cnt=1;
	
	mp=new((byte*)MPAREA_BASE) MP();
	*/

	BugCheck(not_implemented,0);
}

/*
__declspec(noreturn)
void AP_entry(word pid){
	
	
	buildIDT(pid);
	apic=new((byte*)APIC_PBASE) APIC;
	
	*((byte*)(sysinfo+1)+pid) = apic->id();
	assert(pid,sysinfo->MP_cnt);
	sysinfo->MP_cnt++;
	
	BugCheck(not_implemented,0);
}
*/
