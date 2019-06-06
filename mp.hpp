#pragma once
#include "types.hpp"
#include "assert.hpp"


namespace UOS{
	volatile class MP{
		word command;
		word count;
		/*
		00	release
		
		
		
		FF	lock
		*/
		
		void trigger(void);
		
	public:
		
		void lock(void);
		void unlock(void);
		bool lock_state(void) const;
		void reply(void);
		
	};
	
	
	extern MP* mp;
	
	
}