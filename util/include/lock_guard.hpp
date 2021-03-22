#pragma once
#include "types.h"
#include "util.hpp"

#ifdef UOS_KRNL
#include "intrinsics.hpp"
#endif

namespace UOS{
	
	template<typename M>
	class lock_guard{
		M* mutex;
	public:
		template<typename ... Arg>
		lock_guard(M& m,Arg&& ... args) : mutex(&m){
			m.lock(forward<Arg>(args)...);
		}
		~lock_guard(void){
			if (mutex)
				mutex->unlock();
		}
		void drop(void){
			mutex = nullptr;
		}
	};
#ifdef UOS_KRNL
	template<typename M>
	class interrupt_guard{
		const qword state;
		M* mutex;
	public:
		template<typename ... Arg>
		interrupt_guard(M& m,Arg&& ... args) : state(read_eflags() & 0x0200), mutex(&m){
			cli();
			m.lock(forward<Arg>(args)...);
		}
		~interrupt_guard(void){
			if (mutex)
				mutex->unlock();
			if (state)
				sti();
		}
		void drop(void){
			mutex = nullptr;
		}
	};

	template<>
	class interrupt_guard<void>{
		const qword state;
	public:
		interrupt_guard(void) : state(read_eflags() & 0x0200) {
			cli();
		}
		~interrupt_guard(void){
			if (state)
				sti();
		}
	};
#endif
}