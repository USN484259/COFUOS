

#include "pe.hpp"
#include "list.hpp"
#include "heap.hpp"

using namespace UOS;

extern "C" void buildIDT(...);

void exception_dispatch(qword,qword){}
void interrupt_dispatch(qword){}


heap syspool;


__declspec(noreturn)
void krnlentry(void* module_base){
	
	char strCRT[8]={'.','C','R','T',0};
	fun* globalConstructor = (fun*)peGetSection(module_base,strCRT);
	assert(globalConstructor);
	while(*globalConstructor)
		(*globalConstructor++)();
	
	buildIDT(exception_dispatch,interrupt_dispatch);
	
	new(&syspool) heap();
	
	list<qword> lst;
	auto it=lst.begin();
	lst.push_back(lst.size());
	
}