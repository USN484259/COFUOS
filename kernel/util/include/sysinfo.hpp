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
		qword volume_base;

		word kernel_page;
		word ports[7];

		dword VBE_addr;
		word VBE_mode;
		word VBE_scanline;
		word VBE_width;
		word VBE_height;
		word VBE_bpp;



	} __attribute__((packed));

	extern system_feature features;
}

#define sysinfo ((UOS::SYSINFO const*)SYSINFO_BASE)
