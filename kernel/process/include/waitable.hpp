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
		static thread*& next(thread* th);
	};

	class waitable{
	public:
		enum REASON : byte {NONE = 0, PASSED = 1, NOTIFY = 2, TIMEOUT = 3, ABANDONED = 4};
	protected:
		spin_lock lock;
		dword ref_count;	//????
		thread_queue wait_queue;

		static size_t imp_notify(thread*,REASON);
		static void on_timer(qword,void*);

		//locked before calling, unlock inside
		waitable::REASON imp_wait(qword);
		virtual size_t notify(REASON = NOTIFY);
	public:
		waitable(void) = default;
		waitable(const waitable&) = delete;
		virtual ~waitable(void);
		// (this_thread) waits for (this)
		virtual REASON wait(qword us = 0);
		// notify all in (this)'s queue
		//virtual size_t notify(void);
		void cancel(thread*);



	};
	static_assert(sizeof(waitable) == 0x20,"waitable size mismatch");
}