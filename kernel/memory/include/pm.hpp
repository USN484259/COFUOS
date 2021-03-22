#pragma once
#include "types.h"
#include "util.hpp"
#include "constant.hpp"
#include "sync/include/spin_lock.hpp"
#include "assert.hpp"

namespace UOS{
	class PM{
	public:
		typedef dword (*critical_callback)(PM&,void*);
		enum MODE {NONE = 0,MUST_SUCCEED,TAKE};
	private:
		spin_lock lock;
		word soft_critical_limit;
		word hard_critical_limit;
		qword bmp_size;
		qword total;
		qword used;
		qword reserved;
		qword next;

		critical_callback callback;
		void* userdata;
	
		//static constexpr dword hard_critical_limit = 4;
#ifdef PM_TEST
		void check_integrity(void);
#endif
		void critical_check(void);
	public:
		PM(void);
		qword allocate(MODE = NONE);
		void release(qword);
		bool reserve(dword page_count);
		void set_mp_count(dword);
		void set_critical_callback(critical_callback cb,void* ud);

		inline qword capacity(void) const{
			return total;
		}
		inline qword available(void) const{
			assert(used + reserved <= total);
			return total - used - reserved;
		}
		inline word get_critical_limit(void) const{
			return soft_critical_limit;
		}
		inline qword bmp_page_count(void) const{
			return align_up(bmp_size,PAGE_SIZE) >> 12;
		}

		static bool peek(void* dest,qword paddr,size_t count);
	};
	extern PM pm;
}

