#include "types.hpp"
#include "util.hpp"
#include "SYSINFO.hpp"
#include "mp.hpp"
#include "apic.hpp"
#include "heap.hpp"

namespace UOS{
	
	
	SYSINFO* sysinfo=(SYSINFO*)HIGHADDR(0x0800);

	MP* mp=nullptr;

	APIC* apic=nullptr;

	heap syspool;

	
	
}