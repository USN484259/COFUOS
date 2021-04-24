#pragma once
#include "types.h"
#include "thread.hpp"
#include "memory/include/vm.hpp"
#include "pe64.hpp"
#include "filesystem/include/file.hpp"
#include "assert.hpp"
#include "literal.hpp"
#include "hash_set.hpp"
#include "id_gen.hpp"
#include "interface/include/bridge.hpp"
#include "sync/include/rwlock.hpp"

namespace UOS{
	class handle_table{
		static constexpr dword limit = 0x800;
		static constexpr dword avl_base = 4;
		static constexpr dword handle_of_page = PAGE_SIZE/sizeof(waitable*);
		rwlock objlock;
		dword count = 0;
		dword top = 0;
		waitable** table[align_up(limit*sizeof(waitable*),PAGE_SIZE)/PAGE_SIZE] = {0};
	public:
		handle_table(void) = default;
		handle_table(const handle_table&) = delete;
		~handle_table(void);
		void clear(void);
		dword put(waitable*);
		bool assign(dword,waitable*);
		bool close(dword);
		waitable* operator[](dword) const;
		inline dword size(void) const{
			return count;
		}
		inline bool try_lock(void){
			return objlock.try_lock(rwlock::SHARED);
		}
		inline void lock(void){
			objlock.lock(rwlock::SHARED);
		}
		inline void unlock(void){
			assert(objlock.is_locked() && !objlock.is_exclusive());
			objlock.unlock();
		}
		inline bool is_locked(void) const{
			return objlock.is_locked();
		}
		inline bool is_exclusive(void) const{
			return objlock.is_exclusive();
		}
	};
	class process : public waitable{
		struct hash{
			UOS::hash<dword> h;
			qword operator()(const process& obj){
				return h(obj.id);
			}
			qword operator()(dword id){
				return h(id);
			}
		};
		struct equal{
			bool operator()(const process& ps,dword id){
				return id == ps.id;
			}
		};
		enum STATE : byte {RUNNING,STOPPED};
		friend class process_manager;
		friend struct hash;
		friend struct equal;
		friend void ::UOS::process_loader(qword,qword,qword,qword);
		friend void ::UOS::user_entry(qword,qword,qword,qword);

		struct initial_process_tag {};
		static id_gen<dword> new_id;
	public:
		const dword id;
		dword result = NO_RESOURCE;
		virtual_space* const vspace;
	private:
		volatile STATE state = RUNNING;
		PRIVILEGE privilege = NORMAL;
		dword active_count = 0;
		const PE64* image = nullptr;
		hash_set<thread, thread::hash, thread::equal> threads;
	public:
		literal const commandline;
		handle_table handles;
		const qword start_time;
		volatile qword cpu_time = 0;

		struct startup_info{
			PRIVILEGE privilege;
			stream* std_stream[3];
		};
	public:
		process(initial_process_tag);
		process(literal&& cmd,file* f,const startup_info& info,const qword* args);
		~process(void);
		OBJTYPE type(void) const override{
			return PROCESS;
		}
		bool check(void) override{
			return state == STOPPED;
		}
		inline bool operator==(dword id) const{
			return id == this->id;
		}
		inline size_t size(void) const{
			return active_count;
		}
		inline PRIVILEGE get_privilege(void) const{
			return privilege;
		}
		inline qword get_stack_preserve(void) const{
			return image->stk_reserve;
		}
		inline bool get_result(dword& val){
			if (state != STOPPED)
				return false;
			val = result;
			return true;
		}
		thread* find(dword tid,bool acquire);
		REASON wait(qword = 0,wait_callback = nullptr) override;
		bool relax(void) override;
		void manage(void* = nullptr) override;
		thread* spawn(thread::procedure entry,const qword* args,qword stk_size = 0);
		void kill(dword ret_val);
		//on thread exit
		void on_exit(void);
		//delete thread object
		void erase(thread* th);
	};

	class process_manager{
		spin_lock lock;
		hash_set<process, process::hash, process::equal> table;

	public:
		process_manager(void);
		//thread* get_initial_thread(void);
		inline size_t size(void) const{
			return table.size();
		}
		// std streams should be acquired in advance
		process* spawn(literal&& command,literal&& env,const process::startup_info& info);
		void erase(process* ps);
		bool enumerate(dword& id);
		process* find(dword id,bool acquire);
	};
	extern process_manager proc;
	
}