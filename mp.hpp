#pragma once
#include "types.hpp"
#include "assert.hpp"


namespace UOS{
	volatile class MP{
		word guard;
		word owner;
		word command;
		word count;
		/*
		00	release
		
		
		
		FF	lock
		*/
		
		void trigger(void)volatile;
		
	public:
		enum CMD : word {resume=0xF0,pause=0xFF};
		
		
		MP(void);
		
		void lock(void)volatile;
		void unlock(void)volatile;
		bool lock_state(void) const volatile;
		void sync(CMD,void* = nullptr,size_t = 0)volatile;
		void reply(CMD)volatile;
		
	};
	
	
	extern volatile MP* mp;
	
	
}