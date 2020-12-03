#pragma once
#include "types.hpp"
#include "../../sync/include/spin_lock.hpp"
#include "lock_guard.hpp"
#include "../../image/include/pe.hpp"
//#include "queue.hpp"
#include "constant.hpp"


namespace UOS{



	namespace VM{
		void* map_view(qword pa,qword attrib = PAGE_NX | PAGE_WRITE);
		void unmap_view(void* view);


		//enum attribute : qword {page_write=0x02,page_user=0x04,page_writethrough=0x08,page_cachedisable=0x10,page_global=0x100,page_noexecute=0x8000000000000000};

		class kernel_vspace{
			spin_lock lock;
		public:
			kernel_vspace(void);	//set up reserve-able range
			qword reserve(qword addr,size_t page_count);
			void commit(qword addr,size_t page_count);
			void protect(qword addr,size_t page_count,qword attrib);
			void decommit(qword addr,size_t page_count);
			void release(qword addr,size_t page_count);

			bool peek(void* dst,qword addr,size_t count);
		};


		class virtual_space{
			volatile qword* view_pl4t;
			volatile qword* view_pdpt_user;
		public:
			virtual_space(qword cr3);
			virtual_space(volatile qword* pl4t_view);
			~virtual_space(void);
			void switch_to_user(void);
			qword reserve(qword addr,size_t page_count);
			void commit(qword va,size_t page_count);
			void protect(qword va,size_t page_count,qword attrib);
			void decommit(qword base,size_t page_count);
			void release(qword addr,size_t page_count);

			bool peek(void* dst,qword va,size_t len);

		};
		
		
	}
	extern VM::kernel_vspace vm;
	

}
