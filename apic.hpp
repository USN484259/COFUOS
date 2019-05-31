#pragma once

#include "types.hpp"

namespace UOS{

	class APIC{
		byte const* base;	//virtual base
	
		static const size_t ICR;
		
		dword read(size_t);
		void write(size_t,dword);
		
		public:
		APIC(byte*);
		//~APIC(void);
		
		
		
		
	};




}
