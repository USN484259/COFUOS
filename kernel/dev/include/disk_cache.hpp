#pragma once
#include "types.h"
#include "constant.hpp"
#include "sync/include/rwlock.hpp"
#include "sync/include/event.hpp"

namespace UOS{
	class disk_cache{

		struct slot{
			void* const access;
			const dword phy_page;
			qword lba_base;
			byte valid = 0;
			byte dirty = 0;
			//STATE state = INVALID;
			qword timestamp = 0;
			rwlock objlock;

			slot(qword va);
			slot(const slot&) = delete;

			inline void lock(rwlock::MODE mode){
				objlock.lock(mode);
			}
			inline bool try_lock(rwlock::MODE mode){
				return objlock.try_lock(mode);
			}
			inline void unlock(void){
				objlock.unlock();
			}
			inline bool is_locked(void) const{
				return objlock.is_locked();
			}

			bool match(qword aligned_lba) const;
			void flush(void);
			//X locked before calling, downgrades to S lock
			void reload(qword aligned_lba);
			//S locked before calling
			void load(qword lba,byte count);
			//X locked before calling
			void store(qword lba,byte count);

			void* data(qword lba) const;
		};

		event slot_guard;
		const word slot_count;
		//rwlock cache_guard;
		slot* const table;

		slot* get(qword lba,byte count,bool write);
		void relax(slot* ptr);
	public:
		disk_cache(word slot_count);
		byte read(qword lba,byte count,void* buffer);
		byte write(qword lba,byte count,const void* buffer);
	};
	extern disk_cache dm;
}