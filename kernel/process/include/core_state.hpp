#pragma once
#include "types.hpp"
#include "thread.hpp"

namespace UOS{
	struct core_state {
		word uid;
		word slice;
		dword reserved;
		thread* this_thread;
		thread* fpu_owner;
	};

	class scheduler{
	public:
		static constexpr qword slice_us = 4000;
		static constexpr dword max_slice = 0x10;
		static constexpr word max_priority = 4;
	
	private:
		spin_lock lock;
		thread_queue ready_queue[max_priority];

	public:
		void put(thread*);
		thread* get(word level = max_priority);
	};

	class core_manager{
		dword count;
		core_state* core_list;
		qword timer_ticket;

		static void on_timer(qword,void*);
	public:
		core_manager(void);
		core_state* get(void);

	};

	struct this_core{
		this_core(void);
		this_core(const this_core&) = delete;
	
		word id(void);

		word slice(void);
		void slice(word);

		thread* this_thread(void);

		thread* fpu_owner(void);
		void fpu_owner(thread*);

		void switch_to(thread*);
		static void irq_switch_to(byte,void*);
	};

	extern scheduler ready_queue;
	extern core_manager cores;
}

