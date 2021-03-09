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
		enum REASON : byte {NONE = 0, PASSED = 1, NOTIFY = 2, TIMEOUT = 3};
		enum TYPE {UNKNOWN,THREAD,PROCESS,FILE,MUTEX,EVENT};
	protected:
		spin_lock rwlock;
		dword ref_count = 1;
		thread_queue wait_queue;

		static size_t imp_notify(thread*);
		static void on_timer(qword,void*);

		//locked before calling, unlock inside
		waitable::REASON imp_wait(qword);
		virtual size_t notify(void);
	public:
		waitable(void) = default;
		waitable(const waitable&) = delete;
		virtual ~waitable(void);
		virtual TYPE type(void) const = 0;
		// (this_thread) waits for (this)
		virtual REASON wait(qword us = 0);
		void cancel(thread*);
		void acquire(void);
		//return true if still have reference
		virtual bool relax(void);
		inline dword get_reference_count(void) const{
			return ref_count;
		}
	};
	static_assert(sizeof(waitable) == 0x20,"waitable size mismatch");
}