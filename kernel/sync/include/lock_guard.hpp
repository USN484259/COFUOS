#pragma once
#include "types.hpp"
#include "intrinsics.hpp"

namespace UOS{
	
	template<typename M>
	class lock_guard{
		M& mutex;
	public:
		lock_guard(M& m) : mutex(m){
			m.lock();
		}
				
		~lock_guard(void){
			mutex.unlock();
		}
		
	};

	template<typename M>
	class interrupt_guard{
		const qword state;
		M& mutex;
	public:
		interrupt_guard(M& m) : state(read_eflags() & 0x0200), mutex(m){
			do{
				cli();
				if (m.try_lock())
					break;
				if (state){
					sti();
				}
				mm_pause();
			}while(true);
		}
		~interrupt_guard(void){
			mutex.unlock();
			if (state)
				sti();
		}
	};

	template<>
	class interrupt_guard<void>{
		const qword state;
	public:
		interrupt_guard(void) : state(read_eflags() & 0x0200) {}
		~interrupt_guard(void){
			if (state)
				sti();
		}
	};

}