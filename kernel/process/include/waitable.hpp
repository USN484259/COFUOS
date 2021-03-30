#pragma once
#include "types.h"
#include "interface/include/interface.h"
#include "sync/include/spin_lock.hpp"

namespace UOS{
	class thread;

	struct thread_queue{
		thread* head = nullptr;
		thread* tail = nullptr;

		bool empty(void) const;
		void put(thread*);
		thread* get(void);
		void clear(void);
		static thread*& next(thread* th);
	};

	class waitable{
	public:
		typedef void (*wait_callback)(void);
	protected:
		spin_lock objlock;
		dword ref_count = 0;
		thread_queue wait_queue;

		static size_t imp_notify(thread*,REASON);
		static void on_timer(qword,void*);

		//locked before calling, unlock inside
		REASON imp_wait(qword);
		//locked before calling, unlock inside
		size_t notify(REASON = NOTIFY);
	public:
		waitable(void) = default;
		waitable(const waitable&) = delete;
		virtual ~waitable(void);
		virtual OBJTYPE type(void) const = 0;
		//returns true if signaled (aka PASSED on wait)
		virtual bool check(void) = 0;
		// (this_thread) waits for (this)
		virtual REASON wait(qword us = 0,wait_callback func = nullptr);
		void cancel(thread*);
		bool acquire(void);
		//return true if still have reference
		virtual bool relax(void);
		virtual void manage(void* = nullptr);
		inline dword get_reference_count(void) const{
			return ref_count;
		}
	};
	static_assert(sizeof(waitable) == 0x20,"waitable size mismatch");

	class stream : public waitable{
	public:
		OBJTYPE type(void) const override{
			return OBJTYPE::STREAM;
		}
		virtual IOSTATE state(void) const = 0;
		virtual dword read(void* dst,dword length) = 0;
		virtual dword write(void const* sor,dword length) = 0;
	};
}