#pragma once
#include "types.hpp"
#include "constant.hpp"
#include "..\..\sync\include\spin_lock.hpp"

namespace UOS{
	
	class PM{
	public:
		enum : dword{zero_page=0x01,must_succeed=0x80};
		enum : qword{nowhere = HIGHADDR(0)};
		

	private:
		
		spin_lock m;
		
		
		struct chunk_info{	//0x200 pages
			dword index:13;
			dword avl_count:9;
			dword avl_head:9;
			dword present:1;
			
			chunk_info(void);
			
			bool loaded(void) const;
			
			void load(void);
			void unload(void);
			
			void* get(qword);
			qword put(void* pm);
			
			qword& at(size_t);
			qword at(size_t) const;
			
			
		}*chunk;
		
		//queue<chunk_info> layout;
		
		struct PMMSCAN{
			qword base;
			qword length;
			enum : qword {}type;
		}*layout;
		
		
		
		void* legacy_allocate(volatile void* va);
		void legacy_release(volatile void* ptr);
		
		
	public:
		
			
		PM(void);
		
		
		static void construct(const void*);
		
		static bool spy(void* dst,qword base,size_t len);
		
		void* allocate(volatile void* va,dword = 0);
		void release(volatile void*);
	};
	
	extern PM pmm;
	
	
	//PM::allocate_type operator| (const PM::allocate_type& a,const PM::allocate_type& b);

}
	

