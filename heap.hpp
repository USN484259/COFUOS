#pragma once
#include "types.hpp"
#include "lock.hpp"

void* operator new(size_t);
void* operator new(size_t,void*);
void operator delete(void*,size_t);

void* operator new[](size_t);
void operator delete[](void*);
	

namespace UOS{
	class heap{
		struct node{
			node* prev;
			node* next;
						
		};
		typedef unsigned BLOCK;
		//enum BLOCK{b32=0,b64,b128,b256,b512,k1,k2,k4,k8,k16,k32,k64,k128,k256,k512,m1,nomem};
		
		static const BLOCK bitoff,nomem;
		
		node* pool[16];
		mutex m;
		
	protected:
	
		static size_t align_mask(BLOCK);
		static size_t align_size(BLOCK);
		
		static BLOCK category(size_t);
		
		void* get(BLOCK);
		void put(void*,BLOCK);
		
		
	public:
		heap(void);
		heap(void* base,size_t len);
		
		bool expand(void* base,size_t len);
		void* allocate(size_t req);
		void release(void* base,size_t len);
		
		
	};
	
	extern heap syspool;
	
	
}