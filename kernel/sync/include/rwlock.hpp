#pragma once
#include "types.h"
#include "process/include/waitable.hpp"

namespace UOS{
	class rwlock : public waitable{
		thread* owner = nullptr;
		dword share_count = 0;
	public:
		enum MODE {EXCLUSIVE = 0, SHARED = 1};
	public:
		rwlock(void);
		~rwlock(void);
		OBJTYPE type(void) const override{
			return UNKNOWN;
		}
		bool check(void) override{
			return owner == nullptr;
		}
		bool try_lock(MODE = EXCLUSIVE);
		void lock(MODE = EXCLUSIVE);
		void unlock(void);
		void upgrade(void);
		bool is_locked(void) const;
		bool is_exclusive(void) const;
	};
}