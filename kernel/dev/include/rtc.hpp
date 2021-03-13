#pragma once
#include "types.h"

namespace UOS{
	class RTC{
		qword time = 0;
		byte mode;
		byte century_index = 0;
		bool reset = true;

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
	};
	extern RTC rtc;
}