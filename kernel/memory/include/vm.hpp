#pragma once
#include "types.hpp"
#include "lock_guard.hpp"
#include "..\..\image\include\pe.hpp"
//#include "queue.hpp"
#include "constant.hpp"

#define PAGE_PRESENT 0x01
#define PAGE_WRITE 0x02
#define PAGE_USER 0x04
#define PAGE_WT 0x08
#define PAGE_CD 0x10


#define PAGE_LARGE 0x80
#define PAGE_GLOBAL 0x100

//UOS defined indicates PM::release needed
#define PAGE_COMMIT	0x4000000000000000

#define PAGE_NX 	0x8000000000000000

namespace UOS{

	void invlpg(volatile void*);

	namespace VM{
		
		//enum attribute : qword {page_write=0x02,page_user=0x04,page_writethrough=0x08,page_cachedisable=0x10,page_global=0x100,page_noexecute=0x8000000000000000};
		bool spy(void* dst,qword base,size_t len);
		
		
		/*
		
		1G page pointed by PDPT
		self-contained VMG info(PDT and bitmap)
		
		Page_head + 16*offset (pages):
		GAP
		PDT page
		bitmap[8]
		PT0 page
		GAP / PTn
		//avl
		
		12 pages in total
		*/
		
		
		class VMG{
			volatile qword present:1;
			volatile qword writable:1;
			volatile qword user:1;
			volatile qword writethrough:1;
			volatile qword cachedisable:1;
			volatile qword accessed:1;
			volatile qword highaddr:1;		//UOS defined indicates system area
			volatile qword largepage:1;
			volatile qword offset:4;		//UOS defined indicates VMG info position
			volatile qword pmpdt:40;
			volatile qword index:9;			//UOS defined index of self in PDPT (#-th GB in the area)
			volatile qword sync:1;			//UOS defined sync lock for page table operation
			volatile qword :1;
			volatile qword xcutedisable:1;
			
			class PT_mapper{
				volatile VMG& vmg;
				volatile qword* base;
			
			public:
				PT_mapper(volatile VMG&,size_t);
				PT_mapper(volatile VMG&,void*);
				~PT_mapper(void);
			
				volatile qword& operator[](size_t)volatile;
			
			};
		

			friend class lock_guard<volatile VMG>;
			friend class PT_mapper;
			
			void lock(void)volatile;
			void unlock(void)volatile;

			volatile byte* base(void)const volatile;		//LA of this GB
			volatile qword* table(void)const volatile;	//LA of PDT
			volatile byte* bitmap(void)const volatile;	//LA of bitmap
			
			void check_range(volatile void*,size_t) const volatile;	//check if within this VMG
			
			bool bitset(size_t,size_t)volatile;
			bool bitscan(size_t&,size_t)volatile;
			void bitclear(size_t,size_t)volatile;
			bool bitcheck(size_t,size_t) const volatile;
			
			qword PTE_set(volatile qword& dst,void* pm,qword attrib)volatile;
			
			
		public:
			static void construct(const PE64&);
			VMG(bool,word);
			~VMG(void);
			void* reserve(void* fixbase,size_t pagecount)volatile;
			bool commit(void* base,size_t pagecount,qword attrib)volatile;
			bool map(void* vbase,void* pm,qword attrib)volatile;
			void release(void* base,size_t pagecount)volatile;
			qword protect(void* base,size_t pagecount,qword attrib)volatile;	//attrib==0 means query
			
			
			// get_PM ?
			
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
