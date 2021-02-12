#pragma once
#include "types.hpp"
#include "sync/include/spin_lock.hpp"
#include "process/include/waitable.hpp"
#include "hash.hpp"
#include "hash_set.hpp"

namespace UOS{
	class basic_timer{
	public:
		static constexpr qword us2fs = (qword)1000*1000*1000;
		static constexpr qword heartbeat_rate_us = 80;
		typedef void (*CALLBACK)(qword,void*);
	private:
		/*
		struct record_type{
			waitable* ptr;
			qword ticket;

			record_type(waitable* p, qword t) : ptr(p), ticket(t) {}
			
			struct hash{
				inline qword operator()(const record_type& obj){
					return (qword)obj.ptr;
				}
				inline qword operator()(waitable const* ptr){
					return (qword)ptr;
				}
			};
			struct equal{
				inline bool operator()(const record_type& obj,waitable const* cmp){
					return obj.ptr == cmp;
				}
			};
		};
		*/

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
		qword volatile* base;
		dword tick_fs;
		byte channel_index;

		//qword tick_count;
		qword conductor;
		hash_set<qword,UOS::hash<qword>,UOS::equal_to<qword> > record;
		linked_list<queue_type> delta_queue;

		static void irq_timer(byte,void*);
		void on_timer(void);
	public:
		basic_timer(void);
		basic_timer(const basic_timer&) = delete;
		qword wait(qword us, CALLBACK func, void* arg, bool repeat = false);
		bool cancel(qword ticket);
	};
	extern basic_timer timer;
}