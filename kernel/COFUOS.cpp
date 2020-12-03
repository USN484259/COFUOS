#include "types.hpp"
#include "assert.hpp"
#include "cpu/include/hal.hpp"
#include "constant.hpp"
#include "sysinfo.hpp"
#include "image/include/pe.hpp"
#include "memory/include/vm.hpp"
#include "memory/include/pm.hpp"
#include "memory/include/heap.hpp"
#include "exception/include/kdb.hpp"
#include "lang.hpp"

using namespace UOS;


//[[ noreturn ]]
//void AP_entry(word);

byte unitest_buffer[PAGE_SIZE];

[[ noreturn ]]
void krnlentry(void* module_base){
	buildIDT();

#ifdef ENABLE_DEBUGGER
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

	//TEST pm
	zeromemory(unitest_buffer,PAGE_SIZE);
	__debugbreak();
	
	size_t page_count = 0;
	do{
		qword addr = pm.allocate(0x0F);
		dbgprint("allocated %p, %d/%d",addr,pm.available(),pm.capacity());
		if (0 == addr)
			break;
		addr >>= 12;
		unitest_buffer[addr >> 3] |= (1 << (addr & 7));
		++page_count;
	}while(true);
	__debugbreak();

	static const qword table[] = {0x316,0x315};
	for (auto index : table){
		dbgprint("releasing page %x, %d remaining",index,--page_count);
		unitest_buffer[index >> 3] &= ~(1 << (index & 7));
		pm.release(index << 12);
	}
	__debugbreak();
	while(page_count){
		qword tsc = __rdtsc();
		dbgprint("random from TSC : %x",tsc);
		tsc &= ((1 << 15) - 1);
		auto index = tsc;
		do{
			if (unitest_buffer[index >> 3] & (1 << (index & 7)))
				break;
			if (++index == (1 << 15))
				index = 0;
			if (index == tsc){
				dbgprint("page not found");
				BugCheck(corrupted,tsc);
			}
		}while(true);
		dbgprint("releasing page %x, %d remaining",index,--page_count);
		unitest_buffer[index >> 3] &= ~(1 << (index & 7));
		pm.release(index << 12);


	}
	__debugbreak();

	BugCheck(not_implemented,0);
}

