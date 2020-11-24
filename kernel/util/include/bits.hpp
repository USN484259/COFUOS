#pragma once
#include "types.hpp"

namespace UOS{
	class bits{
		byte* base;
		size_t length;
		
	public:
		bits(void*,size_t);
		
		int at(size_t) const;
		void at(size_t,int);

		void set(size_t,size_t,int);
		
	};
}