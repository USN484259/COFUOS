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

#ifdef PM_TEST
		void check_integration(void);
#endif

	public:
		PM(void);
		qword allocate(byte tag = 0);
		void release(qword);
		
		qword capacity(void) const;
		qword available(void) const;

		static bool peek(void* dest,qword paddr,size_t count);
	};
	extern PM pm;
}

