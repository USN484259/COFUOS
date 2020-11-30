#pragma once
#include "types.hpp"

namespace UOS{
	struct SYSINFO{
		qword sig;
		qword reserved_1;

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
	};
#define sysinfo ((SYSINFO*)SYSINFO_BASE)
}
