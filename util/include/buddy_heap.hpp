#pragma once
#include "types.h"


namespace UOS{
	//non-ring bi-directional linked list
	class buddy_heap {
	public:
		typedef void* (*EXPANDER)(size_t&);
	private:
		struct node{
			node* prev;
			node* next;
		};
		typedef unsigned BLOCK;
		//enum BLOCK{b32=0,b64,b128,b256,b512,k1,k2,k4,k8,k16,k32,k64,k128,k256,k512,m1,nomem};
		
		enum : BLOCK { bitoff = 5, nomem = 16 };

		enum : size_t {initial_cap = 0x4000};

		mutex lock;
		node* pool[16] = {0};
		size_t cap_size = 0;
		EXPANDER callback = nullptr;

	
		static size_t align_mask(BLOCK);
		static size_t align_size(BLOCK);
		
		static BLOCK category(size_t);
		static BLOCK category(void*,size_t);
		
		void* get(BLOCK);
		void put(void*,BLOCK);
		

	public:
		buddy_heap(EXPANDER = nullptr);

		size_t capacity(void) const;
		size_t max_size(void) const;
		bool expand(void* base,size_t len);
		void* allocate(size_t req);
		void release(void* base,size_t len);
	
	};
	
	extern buddy_heap heap;
	
	
}
