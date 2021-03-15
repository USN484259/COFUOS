#pragma once
#include "types.h"
#include "process/include/waitable.hpp"

namespace UOS{
	class event : public waitable{
		volatile dword state;
	
	public:
		event(bool initial_state = false);
		OBJTYPE type(void) const override{
			return EVENT;
		}
		bool check(void) const override{
			return state;
		}
		REASON wait(qword us = 0,wait_callback = nullptr) override;
		bool signal_one(void);
		size_t signal_all(void);
		void reset(void);
		inline bool get_state(void) const{
			return (state);
		}
	};
}