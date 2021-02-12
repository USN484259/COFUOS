#pragma once
#include "types.hpp"
#include "waitable.hpp"
#include "context.hpp"
#include "intrinsics.hpp"
namespace UOS{
	struct atomic_id {
		volatile qword count = 0;
		qword operator()(void);
	};
	class process;
	class thread : waitable{
		struct hash{
			qword operator()(const thread& obj){
				return obj.id;
			}
			qword operator()(qword id){
				return id;
			}
		};
		struct equal{
			bool operator()(const thread& th,dword id){
				return id == th.id;
			}
		};
		friend class waitable;
		friend class scheduler;
		friend class process;
		friend struct thread_queue;
		friend struct hash;
		friend struct equal;
		friend class conx_off_check;
	public:
		typedef void (*procedure)(void*);
		enum STATE : word {READY,RUNNING,STOPPED,WAITING};

	private:
		const dword id;
		STATE state = READY;
		word priority = 1;
		process& ps;
		thread* next = nullptr;
		waitable* wait_for = nullptr;
		qword timer_ticket = 0;

		context gpr;
		SSE_context* sse = nullptr;

		qword krnl_stk_top;
		qword krnl_stk_reserved;
		
		qword user_stk_top = 0;
		qword user_stk_reserved = 0;

		struct initial_thread_tag {};
		static atomic_id new_id;
	public:
		thread(initial_thread_tag, process&);
		thread(process& owner,procedure entry,void* arg,qword stk_size);
		bool has_context(void) const;
		word get_priority(void) const;
		context const* get_context(void) const;
		process& get_process(void);
		void set_state(STATE);
		void sleep(qword us);
	};
	class conx_off_check{
		static_assert(offsetof(thread,gpr) == CONX_OFF,"CONX_OFF mismatch");
	};
};