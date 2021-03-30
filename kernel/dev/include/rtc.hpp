#pragma once
#include "types.h"

namespace UOS{
	class RTC{
	public:
		typedef void (*callback)(qword,void*);
	private:
		qword time = 0;
		byte mode;
		byte century_index = 0;
		bool reset = true;
		callback volatile func = nullptr;
		void* userdata = nullptr;

		static void on_irq(byte id,void* data);
		//could handle 'overflowed' BCD
		void convert(byte&);
		void update(void);
	public:
		RTC(void);
		inline qword get_time(void) const{
			return time;
		}
		inline void reload(void){
			reset = true;
		}
		void set_handler(callback cb,void* ud);
	};
	extern RTC rtc;
}