#pragma once
#include "types.hpp"
#include "waitable.hpp"
#include "context.hpp"
#include "hash.hpp"
#include "id_gen.hpp"
#include "interface/include/loader.hpp"

namespace UOS{
	class process;
	class thread : public waitable{
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
		friend void ::UOS::userentry(void*);
	public:
		typedef void (*procedure)(void*);
		enum STATE : byte {READY,RUNNING,WAITING,STOPPED};
	public:
		const dword id;
	private:
		volatile STATE state : 4;
		REASON reason : 4;
		byte priority;
		word slice;
		process* const ps;
		thread* next = nullptr;
		waitable* wait_for = nullptr;
		qword timer_ticket = 0;

		qword krnl_stk_top;
		qword krnl_stk_reserved;
		
		context gpr;
		SSE_context* sse = nullptr;

		qword user_stk_top = 0;
		qword user_stk_reserved = 0;
	public:
		qword user_handler = 0;
		
	private:
		struct initial_thread_tag {};
		static id_gen<dword> new_id;
	public:
		thread(initial_thread_tag, process*);
		thread(process* owner,procedure entry,void* arg,qword stk_size);
		~thread(void);
		TYPE type(void) const override{
			return THREAD;
		}
		inline bool has_context(void) const{
			return gpr.rflags != 0;
		}
		inline byte get_priority(void) const{
			return priority;
		}
		inline context const* get_context(void) const{
			return &gpr;
		}
		inline process* get_process(void){
			return ps;
		}
		inline word get_slice(void) const{
			return slice;
		}
		inline void put_slice(word val){
			slice = val;
		}
		inline STATE get_state(void) const{
			return state;
		}
		void set_priority(byte);
		//locked before calling
		bool set_state(STATE,qword arg = 0,waitable* obj = nullptr);
		inline qword get_ticket(void) const{
			return timer_ticket;
		}
		inline REASON get_reason(void) const{
			return reason;
		}
		inline void lock(void){
			rwlock.lock();
		}
		inline bool try_lock(void){
			return rwlock.try_lock();
		}
		inline void unlock(void){
			rwlock.unlock();
		}
		inline bool is_locked(void) const{
			return rwlock.is_locked();
		}
		REASON wait(qword us = 0) override;
		bool relax(void) override;
		void on_stop(void);
		void save_sse(void);
		void load_sse(void);
		static void sleep(qword us);
		static void kill(thread*);
		//[[ noreturn ]]
		//static void exit(void);
	};
	struct conx_off_check{
		static_assert(offsetof(thread,gpr) == CONX_OFF,"CONX_OFF mismatch");
		static_assert(offsetof(thread,krnl_stk_top) == KSP_OFF,"KSP_OFF mismatch");
	};
};