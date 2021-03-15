#pragma once
#include "types.h"
#include "process/include/waitable.hpp"
#include "sync/include/spin_lock.hpp"

namespace UOS{
	class PS_2{
		class safe_queue : public waitable{
			static constexpr dword QUEUE_SIZE = 0x100;
			byte* const buffer;
			dword head;
			dword tail;
		public:
			safe_queue(void);
			safe_queue(const safe_queue&) = delete;
			~safe_queue(void);
			OBJTYPE type(void) const override{
				return UNKNOWN;
			}
			byte get(void);
			void put(byte);
			void clear(void);
			bool check(void) const override{
				return head != tail;
			}
			REASON wait(qword = 0,wait_callback = nullptr) override;
		};

		spin_lock lock;
		struct CHANNEL{
			thread* th = nullptr;
			safe_queue queue;
		}channel[2];

		static void on_irq(byte,void*);
		static void thread_ps2(qword,qword,qword,qword);
		static bool device_keybd(PS_2&,byte);
		static bool device_mouse(PS_2&,byte,byte);
	public:
		PS_2(void);
		PS_2(const PS_2&) = delete;
	};
	extern PS_2 ps2_device;
}