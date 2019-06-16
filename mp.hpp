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
		
		void trigger(void);
		
	public:
		enum CMD : word {resume=0xF0,pause=0xFF};
		
		
		MP(void);
		
		void lock(void);
		void unlock(void);
		bool lock_state(void) const;
		void sync(CMD,void* = nullptr,size_t = 0);
		void reply(CMD);
		
	};
	
	
	extern MP* mp;
	
	
}