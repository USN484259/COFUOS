#pragma once
#include "types.hpp"

namespace UOS{

	class interrupt_guard{
		const qword state;
	public:
		interrupt_guard(void);
		~interrupt_guard(void);
		
	};
	
	
	class mutex{
		volatile dword m;
		
	public:
		mutex(void);
		
		void lock(void)volatile;
		void unlock(void)volatile;
		
		bool lock_state(void) const volatile;
		
		
	};
	
	
	template<typename L>
	class lock_guard{
		volatile L& lck;
	public:
		lock_guard(volatile L& l) : lck(l){
			l.lock();
		}
				
		~lock_guard(void){
			lck.unlock();
		}
		
	};


}