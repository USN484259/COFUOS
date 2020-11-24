#pragma once
#include "types.hpp"

namespace UOS {
	class heap_base{
	public:
		virtual ~heap_base(void) = default;

		//memory managed by this heap
		virtual size_t capacity(void) const = 0;
		
		//general allocate method
		virtual void* allocate(size_t) = 0;
		
		//general release method
		virtual void release(void*,size_t) = 0;
		
		//hint for max possible allocation
		virtual size_t max_size(void) const = 0;
		
		//expand by adding blocks into the heap
		virtual bool expand(void*,size_t) = 0;
	};



}
