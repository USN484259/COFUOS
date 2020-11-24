#include "types.hpp"
#include "util.hpp"
#include "sysinfo.hpp"
//#include "mp.hpp"
//#include "apic.hpp"
#include "memory\include\heap.hpp"
#include "memory\include\pm.hpp"
#include "memory\include\vm.hpp"
#include "constant.hpp"


namespace UOS{
	
	
	volatile SYSINFO* const sysinfo=(SYSINFO*)SYSINFO_BASE;

	//volatile MP* mp=nullptr;

	//volatile APIC* apic=nullptr;
	
	volatile VM::VMG* const VM::sys = (VM::VMG*)HIGHADDR(PDPT_HIGH_PBASE);
	
	
	//queue<PM::chunk_info> PM::layout;
	
	PM pmm;


	alignas(0x100) void* exit_callback[0x20] = {nullptr};
	
}
