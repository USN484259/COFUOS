#pragma once
#include "types.h"
#include "waitable.hpp"
#include "context.hpp"
#include "hash.hpp"
#include "id_gen.hpp"
#include "interface/include/bridge.hpp"

namespace UOS{
	class process;
	byte check_guard_page(qword);
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
		friend void ::UOS::process_loader(qword,qword,qword,qword);
		friend void ::UOS::user_entry(qword,qword,qword,qword);
		friend byte ::UOS::check_guard_page(qword);
	public:
		typedef void (*procedure)(qword,qword,qword,qword);
		enum STATE : byte {READY,RUNNING,WAITING,STOPPED};
		//enum : byte {HOLD_VSPACE = 1,HOLD_HANDLE = 2};
		enum : byte {CRITICAL = 1, KILL = 2};
	public:
		const dword id;
	private:
		volatile STATE state : 4;
		byte critical : 4;
		byte priority;
		REASON reason;
		byte slice;
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
		qword slice_timestamp = 0;
		qword user_handler = 0;
		
	private:
		struct initial_thread_tag {};
		static id_gen<dword> new_id;
	public:
		thread(initial_thread_tag, process*);
		thread(process* owner,procedure entry,const qword* args,qword stk_size);
		~thread(void);
		OBJTYPE type(void) const override{
			return OBJ_THREAD;
		}
		bool check(void) override{
			return state == STOPPED;
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
		inline byte get_slice(void) const{
			return slice;
		}
		inline void put_slice(byte val){
			slice = val;
		}
		inline STATE get_state(void) const{
			return state;
		}
		bool set_priority(byte);
		//locked before calling
		bool set_state(STATE,qword arg = 0,waitable* obj = nullptr);
		inline qword get_ticket(void) const{
			return timer_ticket;
		}
		inline REASON get_reason(void) const{
			return reason;
		}
		inline void lock(void){
			objlock.lock();
		}
		inline bool try_lock(void){
			return objlock.try_lock();
		}
		inline void unlock(void){
			objlock.unlock();
		}
		inline bool is_locked(void) const{
			return objlock.is_locked();
		}
		inline bool is_exclusive(void) const{
			return objlock.is_exclusive();
		}
		REASON wait(qword us = 0,wait_callback = nullptr) override;
		bool relax(void) override;
		void manage(void* = nullptr) override;
		// lock before calling, unlocks inside
		void on_stop(void);
		void on_gc(void);
		void save_sse(void);
		void load_sse(void);
		void hold(void);
		void drop(void);
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