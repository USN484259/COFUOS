#pragma once
#include "types.hpp"
#include "thread.hpp"
#include "image/include/pe.hpp"
#include "hash_set.hpp"
#include "hash.hpp"

namespace UOS{
	constexpr dword initial_pid = 0;

	class process : waitable{
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
		friend class process_manager;
		friend struct hash;
		friend class equal;

		const dword id;
		qword cr3;
		PE64* image;
		hash_set<thread, thread::hash, thread::equal> threads;

		struct initial_process_tag {};
	public:
		process(initial_process_tag);
		bool operator==(dword id) const{
			return id == this->id;
		}
		thread* get_thread(dword tid){
			auto it = threads.find(tid);
			return (it == threads.end()) ? nullptr : &(*it);
		}

		thread* spawn(thread::procedure entry,void* arg,qword stk_size = 0);
		void kill(thread* th);
	};

	class process_manager{
		spin_lock lock;
		hash_set<process, process::hash, process::equal> table;

	public:
		process_manager(void);
		thread* get_initial_thread(void);

	};
	extern process_manager proc;
	
}