#pragma once

#include "types.hpp"

namespace UOS{

	volatile class APIC{
		//byte const* base;	//virtual base

		
		dword read(size_t)volatile;
		
		void write(size_t,dword)volatile;
		
		public:
		APIC(void);
		//~APIC(void);
		byte id(void)volatile;
		void mp_break(void)volatile;
		
		
	};

	extern volatile APIC* apic;


}
