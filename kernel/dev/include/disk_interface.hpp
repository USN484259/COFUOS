#pragma once
#include "types.h"
#include "constant.hpp"
#include "sync/include/rwlock.hpp"
#include "sync/include/event.hpp"

namespace UOS{
	class disk_interface{
	public:
		class slot{
			friend class disk_interface;
			void* const access;
			const dword phy_page;
			qword lba_base;
			byte valid = 0;
			byte dirty = 0;
			qword timestamp = 0;
			rwlock objlock;

			slot(qword va);
			slot(const slot&) = delete;

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

			bool match(qword aligned_lba) const;
			//locked before calling
			bool flush(void);
			//locked before calling
			bool reload(qword aligned_lba);
			//locked before calling
			bool load(qword lba,byte count);
			//locked before calling
			bool store(qword lba,byte count);
		public:
			qword base(void) const{
				return lba_base;
			}
			void* data(qword lba) const;
		};
	private:
		static constexpr qword flush_interval = 1000*1000*4;

		event slot_guard;
		const word slot_count;
		qword ticket = 0;
		event ev_flush;
		thread* th_flush = nullptr;
		slot* const table;

	public:
		disk_interface(word slot_count);
		static byte count(qword lba);

		slot* get(qword lba,byte count,bool write);
		void upgrade(slot* ptr,qword lba,byte count);
		void relax(slot* ptr,bool flush = false);

		void flush(void);
		static void thread_flush(qword,qword,qword,qword);
		static void on_timer(qword,void*);
	};
	extern disk_interface dm;
}