#include "types.hpp"
#include "util.hpp"
#include "sysinfo.hpp"
//#include "mp.hpp"
//#include "apic.hpp"
#include "exception/include/kdb.hpp"
#include "memory/include/heap.hpp"
#include "memory/include/pm.hpp"
#include "memory/include/vm.hpp"
#include "dev/include/acpi.hpp"
#include "exception/include/exception.hpp"
#include "process/include/process.hpp"
#include "process/include/core_state.hpp"
#include "cpu/include/apic.hpp"
#include "dev/include/rtc.hpp"
#include "dev/include/display.hpp"
#include "dev/include/ps_2.hpp"

namespace UOS{
	system_feature features;
	optional<kdb_stub> debug_stub;
	PE64 const* pe_kernel;
	PM pm;
	VM::kernel_vspace vm;
	paired_heap heap([](size_t& req_size) -> void* {
		req_size = align_up(max(req_size,PAGE_SIZE),PAGE_SIZE);
		auto req_page = req_size >> 12;
		auto ptr = vm.reserve(0,req_page);
		if (ptr){
			if (vm.commit(ptr,req_page)){
				dbgprint("HEAP: expand %d pages @ %p",req_page,ptr);
				return (void*)ptr;
			}
			vm.release(ptr,req_page);
		}
		return nullptr;
	});
	ACPI acpi;
	exception eh;
	process_manager proc;
	scheduler ready_queue;

	APIC apic;
	core_manager cores;
	
	// RTC rtc;
	// Display display;
	// Font text_font;
	// PS_2 ps2_device;
}
