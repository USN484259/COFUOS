#pragma once

#include "types.hpp"

namespace UOS{

	volatile class APIC{
		//byte const* base;	//virtual base

		
		dword read(size_t);
		
		void write(size_t,dword);
		
		public:
		APIC(void);
		//~APIC(void);
		byte id(void);
		void mp_break(void);
		
		
	};

	extern APIC* apic;


}
