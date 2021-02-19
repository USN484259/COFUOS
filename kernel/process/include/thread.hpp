#pragma once
#include "types.hpp"
#include "waitable.hpp"
#include "context.hpp"
#include "hash.hpp"

namespace UOS{
	struct atomic_id {
		volatile qword count = 0;
		qword operator()(void);
	};
	class process;
	class thread : waitable{
		struct hash{
			UOS::hash<dword> h;
			qword operator()(const thread& obj){
				return h(obj.id);
			}
			qword operator()(qword id){
				return h(id);
			}
		};
		struct equal{
			bool operator()(const thread& th,dword id){
				return id == th.id;
			}
		};
		//friend class waitable;
		friend class scheduler;
		friend class process;
		friend struct thread_queue;
		friend struct hash;
		friend struct equal;
		friend struct conx_off_check;
	public:
		typedef void (*procedure)(void*);
		enum STATE : byte {READY,RUNNING,STOPPED,WAITING};
		
	private:
		const dword id;
		STATE state = READY;
		REASON reason = NONE;
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
		~thread(void);
		inline dword get_id(void) const{
			return id;
		}
		inline bool has_context(void) const{
			return gpr.rflags != 0;
		}
		inline word get_priority(void) const{
			return priority;
		}
		inline context const* get_context(void) const{
			return &gpr;
		}
		inline process& get_process(void){
			return ps;
		}
		inline STATE get_state(void) const{
			return state;
		}
		void set_priority(word);
		void set_state(STATE,qword arg = 0,waitable* obj = nullptr);
		inline qword get_ticket(void) const{
			return timer_ticket;
		}
		inline REASON get_reason(void) const{
			return reason;
		}
		static void sleep(qword us);
		[[ noreturn ]]
		static void exit(void);
	};
	struct conx_off_check{
		static_assert(offsetof(thread,gpr) == CONX_OFF,"CONX_OFF mismatch");
	};
};