#pragma once
#include "types.h"
#include "constant.hpp"

namespace UOS{
	class system_feature{
		qword state = 0;	
	public:
		enum FEATURE : word {GDB, MEM, APIC, PS, SCR};
		void set(FEATURE);
		void clear(FEATURE);
		bool get(FEATURE) const;
	};
	struct SYSINFO{
		qword sig;
		struct {
			qword address : 56;
			qword version : 8;
		} rsdp;

		qword PMM_avl_top;
		dword kernel_page;
		dword cpuinfo;
		
		word addrwidth;
		word ports[7];
		
		word VBE_mode;
		word VBE_scanline;
		word VBE_width;
		word VBE_height;
		word VBE_bpp;
		word reserved_2;
		dword VBE_addr;
		
		dword FAT_header;
		dword FAT_table;
		dword FAT_data;
		dword FAT_cluster;
	} __attribute__((packed));

	extern system_feature features;
}

#define sysinfo ((UOS::SYSINFO const*)SYSINFO_BASE)
