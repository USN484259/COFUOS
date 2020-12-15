#include "types.hpp"
#include "util.hpp"
#include "sysinfo.hpp"
//#include "mp.hpp"
//#include "apic.hpp"
#include "memory/include/heap.hpp"
#include "memory/include/pm.hpp"
#include "memory/include/vm.hpp"
#include "dev/include/acpi.hpp"
#include "exception/include/exception.hpp"
#include "cpu/include/apic.hpp"

namespace UOS{
	PE64 const* pe_kernel;
	PM pm;
	VM::kernel_vspace vm;
	paired_heap heap([](size_t& req_size) -> void* {
		req_size = align_up(max(req_size,PAGE_SIZE),PAGE_SIZE);
		auto req_page = req_size >> 12;
		auto ptr = vm.reserve(0,req_page);
		if (ptr){
			if (vm.commit(ptr,req_page)){
				return (void*)ptr;
			}
			vm.release(ptr,req_page);
		}
		return nullptr;
	});
	ACPI acpi;
	exception eh;
	APIC apic;
}
