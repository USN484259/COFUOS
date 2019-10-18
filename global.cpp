#include "types.hpp"
#include "util.hpp"
#include "SYSINFO.hpp"
#include "mp.hpp"
#include "apic.hpp"
#include "heap.hpp"
#include "mm.hpp"
#include "constant.hpp"


namespace UOS{
	
	
	volatile SYSINFO* const sysinfo=(SYSINFO*)SYSINFO_BASE;

	volatile MP* mp=nullptr;

	volatile APIC* apic=nullptr;
	
	volatile VM::VMG* const VM::sys = (VM::VMG*)HIGHADDR(PDPT_HIGH_PBASE);
	
	heap syspool;
	
	queue<PM::chunk_info> PM::layout;


	alignas(0x100) void* exit_callback[0x20] = {nullptr};
	
}