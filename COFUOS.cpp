
typedef unsigned long long QWORD;

#include "list.hpp"
#include "heap.hpp"

using namespace UOS;

extern "C" void buildIDT(...);

void exception_dispatch(QWORD,QWORD){}
void interrupt_dispatch(QWORD){}


heap syspool;


__declspec(noreturn)
void krnlentry(QWORD module_base){
	buildIDT(exception_dispatch,interrupt_dispatch);
	
	new(&syspool) heap();
	
	list<QWORD> lst;
	auto it=lst.begin();
	lst.push_back(lst.size());
	
}