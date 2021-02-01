#pragma once
#include "types.hpp"
#include "sync/include/spin_lock.hpp"
#include "image/include/pe.hpp"
//#include "queue.hpp"
#include "pm.hpp"
#include "constant.hpp"

namespace UOS{



	namespace VM{
		class map_view{
			void* view;
			void unmap(void);
		public:
			map_view(void);
			map_view(qword pa,qword attrib = PAGE_XD | PAGE_WRITE);
			~map_view(void);
			void map(qword pa,qword attrib = PAGE_XD | PAGE_WRITE);
			template<typename T>
			operator T*(void) const{
				return (T*)view;
			}

		};

		struct PT {
			qword present : 1;
			qword write : 1;
			qword user : 1;
			qword wt : 1;
			qword cd : 1;
			qword accessed : 1;
			qword dirty : 1;
			qword pat : 1;
			qword global : 1;
			//UOS_defined {
			enum : qword {OFF, SIZE, PREV, NEXT} type : 2;
			qword valid : 1;
			// }
			qword page_addr : 40;
			//UOS_defined {
			
			// union{
			//	qword offset : 9;
			// 	qword size : 9;
			//	qword prev : 9;
			// 	qword next : 9;
			// };
			qword data : 9;
			qword preserve : 1;
			qword bypass : 1;
			// }
			qword xd : 1;
		};

		/*
			To describe a continuous free block
			{NEXT}
			{NEXT PREV}
			{NEXT PREV SIZE}
			{NEXT PREV SIZE OFF...}
			Free blocks linked, ordered by size, ascending
		*/

		struct PDT {
			qword present : 1;
			qword write : 1;
			qword user : 1;
			qword wt : 1;
			qword cd : 1;
			qword accessed : 1;
			qword : 1;
			qword ps : 1;
			//UOS_defined {
			qword remain : 4;
			// }
			qword pt_addr : 40;
			//UOS_defined {
			qword head : 9;
			qword : 1;
			qword bypass : 1;
			// }
			qword xd : 1;
		};

		struct PDPT{
			qword present : 1;
			qword write : 1;
			qword user : 1;
			qword wt : 1;
			qword cd : 1;
			qword accessed : 1;
			qword : 6;
			qword pdt_addr : 40;
			qword : 11;
			qword xd : 1;
		};

		static_assert(sizeof(PT) == 8,"PT size mismatch");
		static_assert(sizeof(PDT) == 8,"PDT size mismatch");
		static_assert(sizeof(PDPT) == 8,"PDPT size mismatch");
		class virtual_space{
		public:
			virtual ~virtual_space(void) = default;
			virtual qword reserve(qword addr,size_t page_count) = 0;
			virtual bool commit(qword addr,size_t page_count) = 0;
			virtual bool protect(qword addr,size_t page_count,qword attrib) = 0;
			virtual bool release(qword addr,size_t page_count) = 0;
			virtual size_t peek(void* dst,qword addr,size_t count) = 0;
		protected:
			//enum PAGE_STATE {FREE,PRESERVE,COMMIT_KRNL,COMMIT_USER};
			typedef bool (*PTE_CALLBACK)(PT& pt,qword addr,qword data);
		protected:
			qword imp_reserve_any(PDT& pdt,PT* table,qword base_addr,word count);
			bool imp_reserve_fixed(PDT& pdt,PT* table,word index,word count);
			//void imp_commit(PDT& pdt,word index,word count,byte = UOS::PM::default_tag);
			//void imp_protect(PDT& pdt,word index,size_t word,qword attrib);
			//void imp_decommit(PDT& pdt,word index,word count);
			void imp_release(PDT& pdt,PT* table,qword base_addr,word count);
			//bool imp_check(PT const* table,word index,word count,PAGE_STATE expect) const;
			size_t imp_iterate(const PDPT* pdpt,qword base_addr,size_t page_count,PTE_CALLBACK callback,qword data = 0);

			struct BLOCK{	//see struct PT
				word self;
				word next;
				word prev;
				word size;
				bool next_valid;
				bool prev_valid;

				void get(const PT* pt);
				void put(PT* pt) const;
			};
			static word get_max_size(const PDT& pdt);
			static void put_max_size(PDT& pdt,word new_value);
			void shift_left(PDT& pdt,PT* table,BLOCK& block);
			void shift_right(PDT& pdt,PT* table,BLOCK& block);
			void insert(PDT& pdt,PT* table,BLOCK& block,word hint);
#ifndef NDEBUG
			void check_integrity(PDT& pdt,PT* table);
#endif
		};

		class kernel_vspace : public virtual_space{
			spin_lock lock;
		public:
			kernel_vspace(void);
			qword reserve(qword addr,size_t page_count) override;
			bool commit(qword addr,size_t page_count) override;
			bool protect(qword addr,size_t page_count,qword attrib) override;
			bool release(qword addr,size_t page_count) override;
			size_t peek(void* dst,qword addr,size_t count) override;
			bool assign(qword va,qword pa,size_t page_count);
		private:
			static bool common_check(qword addr,size_t page_count);
			static void new_pdt(PDPT& pdpt,map_view& view);
			static void new_pt(PDT& pdt,map_view& view);
			bool reserve_fixed(qword addr,size_t page_count);
			qword reserve_any(size_t page_count);
			qword reserve_big(size_t page_count);
			void locked_release(qword addr,size_t page_count);
		};

		/*
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
		
		*/
	}
	extern VM::kernel_vspace vm;
	

}
