#pragma once

#include "types.hpp"

namespace UOS{

	class APIC{
		byte const* base;	//virtual base
	
		static const size_t ICR;
		
		__declspec(noinline)
		dword read(size_t);
		
		__declspec(noinline)
		void write(size_t,dword);
		
		public:
		APIC(byte*);
		//~APIC(void);
		
		
		
		
	};




}
