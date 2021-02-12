#pragma once
#include "types.hpp"
#include "sync/include/spin_lock.hpp"

namespace UOS{
	class thread;

	struct thread_queue{
		thread* head = nullptr;
		thread* tail = nullptr;
	
		void put(thread*);
		thread* get(void);
		void clear(void);
	};

	class waitable{
	protected:
		spin_lock lock;
		dword ref_count;	//????
		thread_queue wait_queue;

		static size_t imp_notify(thread*);
		static void on_timer(qword,void*);
	public:
		virtual ~waitable(void);
		// (this_thread) waits for (this)
		virtual void wait(qword us = 0);
		// notify all in (this)'s queue
		virtual size_t notify(void);
		void cancel(thread*);



	};
	static_assert(sizeof(waitable) == 0x20,"waitable size mismatch");
}