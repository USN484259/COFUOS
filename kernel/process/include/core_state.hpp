#pragma once
#include "types.hpp"
#include "thread.hpp"
#include "intrinsics.hpp"

namespace UOS{
	struct core_state {
		word uid;
		word slice;
		dword reserved;
		thread* this_thread;
		thread* fpu_owner;
		thread* gc_ptr;
	};

	class scheduler{
	public:
		static constexpr qword slice_us = 4000;
		static constexpr dword max_slice = 0x10;
		static constexpr word max_priority = 8;
		// [0] [1..3] [4..6] [7]
		static constexpr word realtime_priority = 0;
		static constexpr word kernel_priority = 2;
		static constexpr word user_priority = 5;
		static constexpr word idle_priority = 7;
	
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
		inline word slice(void){
			return read_gs<word>(offsetof(core_state,slice));
		}
		inline void slice(word val){
			write_gs(offsetof(core_state,slice),val);
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
		void switch_to(thread*);
		[[ noreturn ]]
		void escape(void);
	};

	extern scheduler ready_queue;
	extern core_manager cores;
}

