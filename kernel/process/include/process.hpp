#pragma once
#include "types.hpp"
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
#include "interface/include/loader.hpp"

namespace UOS{
	class handle_table{
		spin_lock rwlock;
		dword count = 0;
		vector<waitable*> table;
	public:
		handle_table(void) = default;
		handle_table(const handle_table&) = delete;
		~handle_table(void);
		void clear(void);
		dword put(waitable*);
		bool close(dword);
		waitable* operator[](dword);
		inline dword size(void) const{
			return count;
		}
		inline bool try_lock(void){
			return rwlock.try_lock(spin_lock::SHARED);
		}
		inline void lock(void){
			rwlock.lock(spin_lock::SHARED);
		}
		inline void unlock(void){
			assert(!rwlock.is_exclusive());
			rwlock.unlock();
		}
		inline bool is_locked(void) const{
			return rwlock.is_locked();
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
		struct startup_info{
			basic_file* file;
			qword image_base;
			dword image_size;
			dword header_size;
			size_t cmd_length;
		};
		enum STATE : byte {RUNNING,STOPPED};
		friend class process_manager;
		friend struct hash;
		friend class equal;
		friend void ::UOS::userentry(void*);
	public:
		const dword id;
		dword result = 0;
		virtual_space* const vspace;
	private:
		const PE64* image = nullptr;
		hash_set<thread, thread::hash, thread::equal> threads;
	public:
		handle_table handles;
	private:
		volatile STATE state = RUNNING;

		struct initial_process_tag {};
		static id_gen<dword> new_id;
	public:
		process(initial_process_tag, kernel_vspace*);
		process(startup_info* info);
		~process(void);
		TYPE type(void) const override{
			return PROCESS;
		}
		inline bool operator==(dword id) const{
			return id == this->id;
		}
		inline thread* get_thread(dword tid){
			auto it = threads.find(tid);
			return (it == threads.end()) ? nullptr : &(*it);
		}
		bool relax(void) override;
		thread* spawn(thread::procedure entry,void* arg,qword stk_size = 0);
		void kill(dword ret_val);
		void erase(thread* th);
	};

	class process_manager{
		spin_lock lock;
		hash_set<process, process::hash, process::equal> table;

	public:
		process_manager(void);
		thread* get_initial_thread(void);
		process* spawn(const string& command);
		void erase(process* ps);
		//ref_count incremented
		process* get(dword id);
	};
	extern process_manager proc;
	
}