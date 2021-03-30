#pragma once
#include "types.h"
#include "process/include/waitable.hpp"

namespace UOS{
	class event : public waitable{
		volatile dword state;
		bool named = false;
	public:
		event(bool initial_state = false);
		~event(void);
		OBJTYPE type(void) const override{
			return EVENT;
		}
		bool check(void) override{
			return state;
		}
		REASON wait(qword us = 0,wait_callback = nullptr) override;
		bool signal_one(void);
		size_t signal_all(void);
		void reset(void);
		bool relax(void) override;
		void manage(void*) override;
	};
}