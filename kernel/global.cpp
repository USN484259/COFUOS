#include "types.h"
#include "util.hpp"
#include "sysinfo.hpp"
#include "assert.hpp"
#include "exception/include/kdb.hpp"
#include "memory/include/heap.hpp"
#include "memory/include/pm.hpp"
#include "memory/include/vm.hpp"
#include "process/include/process.hpp"
#include "process/include/core_state.hpp"
#include "dev/include/acpi.hpp"
#include "dev/include/apic.hpp"
#include "dev/include/timer.hpp"
#include "dev/include/rtc.hpp"
#include "dev/include/display.hpp"
#include "dev/include/ps_2.hpp"
#include "dev/include/pci.hpp"
#include "dev/include/ide.hpp"
#include "dev/include/disk_interface.hpp"
#include "filesystem/include/exfat.hpp"
#include "interface/include/object.hpp"

namespace UOS{
	PE64 const* pe_kernel;
	system_feature features;
	kdb_stub debug_stub(sysinfo->ports[0]);
	PM pm;
	kernel_vspace vm;
	buddy_heap<4,12,spin_lock> heap([](size_t& req_size) -> void* {
		req_size = align_up(max(req_size,PAGE_SIZE),PAGE_SIZE);
		do{
			auto req_page = req_size / PAGE_SIZE;
			auto ptr = vm.reserve(0,req_page);
			if (ptr){
				if (vm.commit(ptr,req_page)){
					dbgprint("HEAP: expand %d pages @ %p",req_page,ptr);
					return (void*)ptr;
				}
				vm.release(ptr,req_page);
			}
			if (req_page <= 1)
				return nullptr;
			req_size = PAGE_SIZE;
		}while(true);
	});
	ACPI acpi;
	PCI pci;
	process_manager proc;
	scheduler ready_queue;

	APIC apic;
	basic_timer timer;
	RTC rtc;
	core_manager cores;
	gc_service gc;
	object_manager named_obj;
	video_memory display;
	PS_2 ps2_device;
	IDE ide;
	disk_interface dm([](qword total) -> word{
		total /= 0x100;
		total = max<qword>(total,8);
		total = min<qword>(total,64);
		return total;
	}(pm.capacity()));
	exfat filesystem(0x10);
}
