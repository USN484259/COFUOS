#pragma once
#include "types.hpp"
#include "constant.hpp"
#include "../../sync/include/spin_lock.hpp"

namespace UOS{
	class PM{
		spin_lock lock;
		qword bmp_size;
		qword total;
		qword used;
		qword next;

		PM(void);
		
	public:
		qword allocate(byte tag = 0);
		void release(qword);

		static bool peek(void* dest,qword paddr,size_t count);
	};
	extern PM pm;
}

