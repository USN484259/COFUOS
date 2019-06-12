#pragma once
#include "types.hpp"
#include "lock.hpp"

namespace UOS{
	
	namespace PM{
		enum type{zero_page=0x01,must_succeed=0x80};
		
		type operator| (const type&,const type&);
		
		
		bool spy(void* dst,qword base,size_t len);
		
		void* allocate(type = (type)0);
		void release(void*);
	}
	
	
	namespace VM{
		
		//enum attribute : qword {page_write=0x02,page_user=0x04,page_writethrough=0x08,page_cachedisable=0x10,page_global=0x100,page_noexecute=0x8000000000000000};
		bool spy(void* dst,qword base,size_t len);
		
		volatile class VMG{
			qword present:1;
			qword writable:1;
			qword user:1;
			qword writethrough:1;
			qword cachedisable:1;
			qword accessed:1;
			qword highaddr:1;
			qword largepage:1;
			qword offset:4;
			qword pmpdt:40;
			qword index:9;
			qword sync:1;
			qword :1;
			qword xcutedisable:1;
			
			
			friend class lock_guard<VMG>;
			void lock(void);
			void unlock(void);

			byte* base(void);
			qword* table(void);
			byte* bitmap(void);
			
			bool bitscan(size_t&,size_t);
			void bitclear(size_t,size_t);
			
			qword PTE_set(volatile qword* dst,void* pm,qword attrib);
			
			
		public:
			VMG(bool,word);
			~VMG(void);
			void* reserve(void* fixbase,size_t pagecount);
			void commit(void* base,size_t pagecount,qword attrib);
			void map(void* vbase,void* pm,qword attrib);
			void release(void* base,size_t pagecount);
			qword protect(void* base,size_t pagecount,qword attrib);	//attrib==0 means query
		};

		
		extern VMG* sys;
		
		
		
		
		class window{
			byte* vbase;
			
		public:
			window(void* pm);
			~window(void);
			
			void zero(void);
			template<typename T>
			T& at(size_t off){
				return *(T*)(vbase+sizeof(T)*off);
			}
			
			
			
		};
		
		
	}
	
	
	


}