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

#ifdef _DEBUG
		void check_integration(void);
#endif

	public:
		static const byte default_tag = 0x10;

		PM(void);
		qword allocate(byte tag = default_tag);
		void release(qword);
		
		qword capacity(void) const;
		qword available(void) const;

		static bool peek(void* dest,qword paddr,size_t count);

		qword bmp_page_count(void) const;
	};
	extern PM pm;
}

