#pragma once
#include "types.hpp"
#include "process/include/waitable.hpp"

namespace UOS{
	class event : public waitable{
		volatile dword state;
	
	public:
		event(bool initial_state = false);
		REASON wait(qword us = 0) override;
		bool signal_one(void);
		size_t signal_all(void);
		void reset(void);
		inline bool get_state(void) const{
			return (state);
		}
	};
}