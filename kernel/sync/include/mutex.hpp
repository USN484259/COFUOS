#pragma once
#include "types.hpp"
#include "process/include/waitable.hpp"

namespace UOS{
	class mutex : public waitable{
		thread* volatile owner = nullptr;
	protected:
		size_t notify(void) override;
	public:
		~mutex(void);
		TYPE type(void) const override{
			return MUTEX;
		}
		REASON wait(qword = 0) override;
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