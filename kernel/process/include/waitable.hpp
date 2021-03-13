#pragma once
#include "types.h"
#include "interface/include/interface.h"
#include "sync/include/spin_lock.hpp"

namespace UOS{
	class thread;
	class handle_table;
	struct thread_queue{
		thread* head = nullptr;
		thread* tail = nullptr;
	
		void put(thread*);
		thread* get(void);
		void clear(void);
		static thread*& next(thread* th);
	};

	class waitable{
	protected:
		mutable spin_lock rwlock;
		dword ref_count = 1;
		thread_queue wait_queue;

		static size_t imp_notify(thread*);
		static void on_timer(qword,void*);

		//locked before calling, unlock inside
		REASON imp_wait(qword);
		virtual size_t notify(void);
	public:
		waitable(void) = default;
		waitable(const waitable&) = delete;
		virtual ~waitable(void);
		virtual OBJTYPE type(void) const = 0;
		//returns true if signaled (aka PASSED on wait)
		virtual bool check(void) const = 0;
		// (this_thread) waits for (this)
		virtual REASON wait(qword us = 0,handle_table* ht = nullptr);
		void cancel(thread*);
		bool acquire(void);
		//return true if still have reference
		virtual bool relax(void);
		inline dword get_reference_count(void) const{
			return ref_count;
		}
	};
	static_assert(sizeof(waitable) == 0x20,"waitable size mismatch");
}