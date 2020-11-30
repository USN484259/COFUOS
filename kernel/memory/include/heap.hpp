#pragma once
#include "types.hpp"
#include "heap_base.hpp"

#ifndef _TEST

#include "../../sync/include/spin_lock.hpp"

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
	class paired_heap : public heap_base{
		struct node{
			node* prev;
			node* next;
						
		};
		typedef unsigned BLOCK;
		//enum BLOCK{b32=0,b64,b128,b256,b512,k1,k2,k4,k8,k16,k32,k64,k128,k256,k512,m1,nomem};
		
		enum : BLOCK { bitoff = 5, nomem = 16 };;
		//static const BLOCK bitoff,nomem;
		
		node* pool[16];
		size_t cap_size;
		spin_lock lock;
		
	protected:
	
		static size_t align_mask(BLOCK);
		static size_t align_size(BLOCK);
		
		static BLOCK category(size_t);
		static BLOCK category(void*,size_t);
		
		void* get(BLOCK);
		void put(void*,BLOCK);
		
		
	public:
		paired_heap(void);
		paired_heap(void* base,size_t len);
		~paired_heap(void) override;
		size_t capacity(void) const override;
		size_t max_size(void) const override;
		bool expand(void* base,size_t len) override;
		void* allocate(size_t req) override;
		void release(void* base,size_t len) override;
	
	};
	
	extern paired_heap heap;
	
	
}
