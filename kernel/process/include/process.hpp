#pragma once
#include "types.hpp"
#include "thread.hpp"
#include "hash_set.hpp"

namespace UOS{
	constexpr dword initial_pid = 0;

	class process : waitable{
		struct hash{
			qword operator()(const process& obj){
				return obj.id;
			}
			qword operator()(qword id){
				return id;
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
		//enum : dword {READY,RUNNING,WAITING} state;
		qword cr3;
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