#pragma once
#include "types.hpp"
#include "constant.hpp"

namespace UOS{
	struct SYSINFO{
		qword sig;
		qword ACPI_RSDT;

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
#define sysinfo ((SYSINFO const*)SYSINFO_BASE)
}
