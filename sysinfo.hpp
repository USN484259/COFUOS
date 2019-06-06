#pragma once
#include "types.hpp"


namespace UOS{
	struct SYSINFO{
		qword sig;
		qword PMM_avl_top;
		
		qword PMM_wmp_vbase;
		dword PMM_wmp_page;
		dword cpuinfo;
		
		word addrwidth;
		word ports[7];
		
		word VBE_bpp;
		word VBE_mode;
		word VBE_width;
		word VBE_height;
		dword VBE_addr;
		dword VBE_lim;
		
		dword FAT_header;
		dword FAT_table;
		dword FAT_data;
		dword FAT_cluster;
		
		qword krnlbase;
		dword AP_entry;
		word MP_cnt;
		word MP_lock;
		//byte[] MP_id
	
	};
	
	extern SYSINFO* sysinfo;


}