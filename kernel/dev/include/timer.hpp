#pragma once
#include "types.h"
#include "sync/include/spin_lock.hpp"
#include "process/include/waitable.hpp"
#include "assert.hpp"
#include "hash_set.hpp"

namespace UOS{
	class basic_timer{
	public:
		static constexpr qword us2fs = (qword)1000*1000*1000;
		static constexpr qword heartbeat_us = 1000;
		static constexpr qword heartbeat_hz = (1000*1000)/heartbeat_us;
		typedef void (*CALLBACK)(qword,void*);
	private:
		struct queue_type{
			qword ticket;
			qword diff_tick;
			CALLBACK func;
			void* arg;
			qword interval;

			queue_type(qword t, qword d,CALLBACK f,void* a,qword i) : \
				ticket(t), diff_tick(d), func(f), arg(a), interval(i) {}
			queue_type(const queue_type&) = delete;
		};

		spin_lock lock;
		dword tick_fs;
		qword volatile* base;
		volatile dword beat_counter;
		qword running_us;
		qword conductor;
		hash_set<qword,UOS::hash<qword>,UOS::equal_to<qword> > record;
		linked_list<queue_type> delta_queue;

		static void irq_timer(byte,void*);
		void on_timer(void);
		void on_second(void);
		void step(unsigned count);
		qword set_timer(unsigned,byte,qword);
		friend class RTC;
	public:
		basic_timer(void);
		basic_timer(const basic_timer&) = delete;
		qword wait(qword us, CALLBACK func, void* arg, bool repeat = false);
		bool cancel(qword ticket);
		inline qword running_time(void) const{
			return running_us;
		}
	};
	extern basic_timer timer;
}