#pragma once
#include "types.hpp"
#include "lock.hpp"

#define PAGE_PRESENT 0x01
#define PAGE_WRITE 0x02
#define PAGE_USER 0x04
#define PAGE_WT 0x08
#define PAGE_CD 0x10


#define PAGE_LARGE 0x80
#define PAGE_GLOBAL 0x100
#define PAGE_COMMIT 0x4000000000000000
#define PAGE_NX 0x8000000000000000



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
		
		
		/*
		
		1G page pointed by PDPT
		self-contained VMG info(PDT and bitmap)
		
		Page_head + 16*offset (pages):
		gap
		PDT page
		bitmap[8]
		//avl
		
		10 pages in total
		*/
		
		
		class VMG{
			qword present:1;
			qword writable:1;
			qword user:1;
			qword writethrough:1;
			qword cachedisable:1;
			qword accessed:1;
			qword highaddr:1;	//UOS defined indicates system area
			qword largepage:1;
			qword offset:4;		//UOS defined indicates VMG info position
			qword pmpdt:40;
			qword index:9;		//UOS defined index of self in PDPT (#-th GB in the area)
			qword sync:1;		//UOS defined sync lock for page table operation
			qword :1;
			qword xcutedisable:1;
			
			
			friend class lock_guard<volatile VMG>;
			void lock(void)volatile;
			void unlock(void)volatile;

			byte* base(void)const volatile;		//LA of this GB
			qword* table(void)const volatile;	//LA of PDT
			byte* bitmap(void)const volatile;	//LA of bitmap
			
			bool bitset(size_t,size_t)volatile;
			bool bitscan(size_t&,size_t)volatile;
			void bitclear(size_t,size_t)volatile;
			
			qword PTE_set(volatile qword& dst,void* pm,qword attrib)volatile;
			
			
		public:
			static void construct(void);
			VMG(bool,word);
			~VMG(void);
			void* reserve(void* fixbase,size_t pagecount)volatile;
			void commit(void* base,size_t pagecount,qword attrib)volatile;
			void map(void* vbase,void* pm,qword attrib)volatile;
			void release(void* base,size_t pagecount)volatile;
			qword protect(void* base,size_t pagecount,qword attrib)volatile;	//attrib==0 means query
		};

		
		extern volatile VMG* const sys;
		
		
		
		
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
			template<typename T>
			const T& at(size_t off) const{
				return *(const T*)(vbase+sizeof(T)*off);
			}
			
			size_t read(void* dst,size_t off,size_t len) const;
			
		};
		
		
	}
	
	
	


}