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
	};

	class waitable{
	protected:
		spin_lock lock;
		dword ref_count;	//????
		thread_queue wait_queue;

		void notify(thread*);
	public:
		virtual ~waitable(void);
		virtual void wait(void);
		//virtual bool notify_one(void);
		//virtual size_t notify_all(void);

	};
	static_assert(sizeof(waitable) == 0x20,"waitable size mismatch");
}