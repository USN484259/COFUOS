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
	
	PM pm;
	VM::kernel_vspace vm;
	paired_heap heap;

}
