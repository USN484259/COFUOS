#pragma once
#include "types.hpp"
#include "thread.hpp"
#include "intrinsics.hpp"

namespace UOS{
	struct core_state {
		word uid;
		thread* this_thread;
		thread* fpu_owner;
		thread* gc_ptr;
	};
	static_assert(offsetof(core_state,this_thread) == 8,"core_state::this_thread mismatch");

	class scheduler{
	public:
		static constexpr qword slice_us = 1000;
		static constexpr word min_slice = 1;
		static constexpr word max_slice = 4;
		static constexpr byte max_priority = 8;
		// [0] [1..3] [4..6] [7]
		static constexpr byte realtime_priority = 0;
		static constexpr byte kernel_priority = 2;
		static constexpr byte user_priority = 5;
		static constexpr byte idle_priority = 7;
	
	private:
		spin_lock lock;
		thread_queue ready_queue[max_priority];

	public:
		void put(thread*);
		thread* get(byte level = max_priority);
	};

	class core_manager{
		dword count;
		core_state* core_list;
		qword timer_ticket;

		static void on_timer(qword,void*);
	public:
		core_manager(void);
		core_state* get(void);
		static void preempt(void);
	};

	class this_core{
		friend class core_manager;
		static void irq_switch_to(byte,void*);
		void gc_service(void);
	public:
		this_core(void)
#ifdef NDEBUG
			= default
#endif
			;
		this_core(const this_core&) = delete;

		inline word id(void){
			return read_gs<word>(offsetof(core_state,uid));
		}
		inline thread* this_thread(void){
			return reinterpret_cast<thread*>(
				read_gs<qword>(offsetof(core_state,this_thread))
			);
		}
		inline thread* fpu_owner(void){
			return reinterpret_cast<thread*>(
				read_gs<qword>(offsetof(core_state,fpu_owner))
			);
		}
		inline void fpu_owner(thread* th){
			write_gs(offsetof(core_state,fpu_owner),
				reinterpret_cast<qword>(th)
			);
		}
		//locks 'th' before calling, unlocks inside
		void switch_to(thread* th);
		//locks 'th' before calling, unlocks inside
		void escape(thread* th);
	};

	extern scheduler ready_queue;
	extern core_manager cores;
}

