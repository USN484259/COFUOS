#include "types.hpp"
#include "util.hpp"
#include "sysinfo.hpp"
//#include "mp.hpp"
//#include "apic.hpp"
#include "memory/include/heap.hpp"
#include "memory/include/pm.hpp"
#include "memory/include/vm.hpp"
#include "constant.hpp"

namespace UOS{
	PE64 const* pe_kernel;
	PM pm;
	VM::kernel_vspace vm;
	paired_heap heap;

}
