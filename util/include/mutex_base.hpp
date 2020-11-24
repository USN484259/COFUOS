#pragma once
#include "types.hpp"

namespace UOS{
	class mutex_base{
	public:
		virtual ~mutex_base(void) = default;
		virtual void lock(void) = 0;
		virtual void unlock(void) = 0;
		virtual bool try_lock(void) = 0;
		virtual bool is_locked(void) const = 0;
	};

}
