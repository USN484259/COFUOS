#pragma once
#include "types.hpp"

#ifndef _TEST

#include "lock.hpp"

void* operator new(size_t);
void* operator new(size_t,void*);
void operator delete(void*,size_t);

void* operator new[](size_t);
void operator delete[](void*);

#else
#include <mutex>

#endif

namespace UOS{
#ifdef _TEST
	using std::mutex;
	template<typename T>
	using lock_guard = std::unique_lock<T>;
#endif
	//non-ring bi-directional linked list
	class heap{
		struct node{
			node* prev;
			node* next;
						
		};
		typedef unsigned BLOCK;
		//enum BLOCK{b32=0,b64,b128,b256,b512,k1,k2,k4,k8,k16,k32,k64,k128,k256,k512,m1,nomem};
		
		enum : BLOCK { bitoff = 5, nomem = 16 };;
		//static const BLOCK bitoff,nomem;
		
		node* pool[16];
		mutex m;
		
	protected:
	
		static size_t align_mask(BLOCK);
		static size_t align_size(BLOCK);
		
		static BLOCK category(size_t);
		static BLOCK category(void*,size_t);
		
		void* get(BLOCK);
		void put(void*,BLOCK);
		
		
	public:
		heap(void);
		heap(void* base,size_t len);
		size_t capability(void) const;
		bool expand(void* base,size_t len);
		void* allocate(size_t req);
		void release(void* base,size_t len);
		
	};
	
	extern heap syspool;
	
	
}