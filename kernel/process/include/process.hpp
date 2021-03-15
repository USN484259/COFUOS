#pragma once
#include "types.h"
#include "thread.hpp"
#include "memory/include/vm.hpp"
#include "pe64.hpp"
#include "filesystem/include/file.hpp"
#include "assert.hpp"
#include "vector.hpp"
#include "string.hpp"
#include "hash_set.hpp"
#include "hash.hpp"
#include "id_gen.hpp"
#include "interface/include/bridge.hpp"
#include "sync/include/rwlock.hpp"

namespace UOS{
	class handle_table{
		rwlock objlock;
		dword count = 0;
		vector<waitable*> table;
	public:
		handle_table(waitable*);
		handle_table(const handle_table&) = delete;
		~handle_table(void);
		void clear(void);
		dword put(waitable*);
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
		friend class equal;
		friend void ::UOS::process_loader(qword,qword,qword,qword);
		friend void ::UOS::user_entry(qword,qword,qword,qword);
	public:
		const dword id;
		dword result = 0;
		virtual_space* const vspace;
	private:
		volatile STATE state = RUNNING;
		PRIVILEGE privilege = NORMAL;
		dword active_count = 0;
		const PE64* image = nullptr;
		hash_set<thread, thread::hash, thread::equal> threads;
	public:
		const string commandline;
		handle_table handles;
		const qword start_time;
		volatile qword cpu_time = 0;

		struct initial_process_tag {};
		static id_gen<dword> new_id;
	public:
		process(initial_process_tag);
		process(const UOS::string& cmd,basic_file* file,const qword* info);
		~process(void);
		OBJTYPE type(void) const override{
			return PROCESS;
		}
		bool check(void) const override{
			return state == STOPPED;
		}
		inline bool operator==(dword id) const{
			return id == this->id;
		}
		inline size_t size(void) const{
			return active_count;
		}
		inline thread* get_thread(dword tid){
			auto it = threads.find(tid);
			return (it == threads.end()) ? nullptr : &(*it);
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
		REASON wait(qword = 0,wait_callback = nullptr) override;
		bool relax(void) override;
		HANDLE spawn(thread::procedure entry,const qword* args,qword stk_size = 0);
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
		thread* get_initial_thread(void);
		inline size_t size(void) const{
			return table.size();
		}
		//string should be in kernel space
		HANDLE spawn(string&& command,string&& env = string());
		void erase(process* ps);
		bool enumerate(dword& id);
		//ref_count incremented
		process* get(dword id);
	};
	extern process_manager proc;
	
}