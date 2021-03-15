#pragma once
#include "types.h"
#include "process/include/waitable.hpp"

namespace UOS{
	class mutex : public waitable{
		thread* volatile owner = nullptr;
	protected:
		size_t notify(void) override;
	public:
		~mutex(void);
		OBJTYPE type(void) const override{
			return MUTEX;
		}
		bool check(void) const override{
			return owner == nullptr;
		}
		REASON wait(qword = 0,wait_callback = nullptr) override;
		void lock(void);
		void unlock(void);
		bool try_lock(void);
		inline bool is_locked(void) const{
			return owner != nullptr;
		}
		inline thread* get_owner(void) const{
			return owner;
		}
	};
}