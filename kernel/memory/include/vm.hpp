#pragma once
#include "types.h"
#include "sync/include/spin_lock.hpp"
#include "pe64.hpp"
#include "pm.hpp"
#include "constant.hpp"
#include "sync/include/rwlock.hpp"

namespace UOS{
	class map_view{
		void* view = nullptr;
		void unmap(void);
	public:
		map_view(void) = default;
		map_view(qword pa,qword attrib = PAGE_XD | PAGE_WRITE);
		map_view(const map_view&) = delete;
		~map_view(void);
		void map(qword pa,qword attrib = PAGE_XD | PAGE_WRITE);
		template<typename T>
		inline operator T*(void) const{
			return (T*)view;
		}

	};

	struct PTE {
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

	struct PDTE {
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

	struct PDPTE{
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

	static_assert(sizeof(PTE) == 8,"PTE size mismatch");
	static_assert(sizeof(PDTE) == 8,"PDTE size mismatch");
	static_assert(sizeof(PDPTE) == 8,"PDPTE size mismatch");
	class virtual_space{
	public:
		static constexpr qword size_512G = 0x008000000000ULL;
	protected:
		qword used_pages;
		typedef bool (*PTE_CALLBACK)(PTE& pt,qword addr,qword data);
	public:
		virtual_space(void) = default;
		virtual_space(const virtual_space&) = delete;
		virtual ~virtual_space(void) = default;
		virtual qword get_cr3(void) const = 0;
		virtual qword reserve(qword addr,dword page_count) = 0;
		virtual bool commit(qword addr,dword page_count) = 0;
		virtual bool protect(qword addr,dword page_count,qword attrib) = 0;
		virtual bool release(qword addr,dword page_count) = 0;

		virtual dword write(qword va,const void* data,dword length);
		virtual dword read(qword va,void* buffer,dword length);
		virtual dword zero(qword va,dword length);

		virtual PTE peek(qword va) = 0;
		inline qword usage(void) const{
			return used_pages;
		}
		virtual bool try_lock(void) = 0;
		virtual void lock(void) = 0;
		virtual void unlock(void) = 0;
		virtual bool is_locked(void) const = 0;
		virtual bool is_exclusive(void) const = 0;
	protected:
		//low-level methods
		qword imp_reserve_any(PDTE& pdt,PTE* table,qword base_addr,word count);
		bool imp_reserve_fixed(PDTE& pdt,PTE* table,word index,word count);
		void imp_release(PDTE& pdt,PTE* table,qword base_addr,word count);
		dword imp_iterate(const PDPTE* pdpt,qword base_addr,dword page_count,PTE_CALLBACK callback,qword data = 0);
		PTE imp_peek(qword va,PDPTE const* pdpt_table);
	protected:
		//generic helper methods
		bool new_pdt(PDPTE& pdpt,map_view& view);
		bool new_pt(PDTE& pdt,map_view& view,bool take);
		bool reserve_fixed(PDPTE* pdpt_table,qword addr,dword page_count);
		qword reserve_any(PDPTE* pdpt_table,dword page_count);
		qword reserve_big(PDPTE* pdpt_table,dword page_count);
		bool safe_release(PDPTE* pdpt_table,qword addr,dword page_count);


		struct BLOCK{	//see struct PTE
			word self;
			word next;
			word prev;
			word size;
			bool next_valid;
			bool prev_valid;

			void get(const PTE* pt);
			void put(PTE* pt) const;
		};
		static word get_max_size(const PDTE& pdt);
		static void put_max_size(PDTE& pdt,word new_value);
		static void resolve_prior(const PDTE& pdt,const PTE* table,BLOCK& block);
		//links kept
		void erase(PDTE& pdt,PTE* table,BLOCK& block);
		//links as hint
		void insert(PDTE& pdt,PTE* table,BLOCK& block);

		//void shift_left(PDTE& pdt,PTE* table,BLOCK& block);
		//void shift_right(PDTE& pdt,PTE* table,BLOCK& block);
		//void insert(PDTE& pdt,PTE* table,BLOCK& block);
#ifdef VM_TEST
		void check_integrity(PDTE& pdt,PTE* table);
#endif
	};

	class kernel_vspace : public virtual_space{
		spin_lock objlock;
		static bool common_check(qword addr,dword page_count);
	public:
		kernel_vspace(void);
		qword get_cr3(void) const override;
		qword reserve(qword addr,dword page_count) override;
		bool commit(qword addr,dword page_count) override;
		bool protect(qword addr,dword page_count,qword attrib) override;
		bool release(qword addr,dword page_count) override;

		dword write(qword va,const void* data,dword length) override;
		dword read(qword va,void* buffer,dword length) override;
		dword zero(qword va,dword length) override;

		PTE peek(qword va) override;
		bool assign(qword va,qword pa,dword page_count);
		bool try_lock(void) override{
			return objlock.try_lock(spin_lock::SHARED);
		}
		void lock(void) override{
			return objlock.lock(spin_lock::SHARED);
		}
		void unlock(void) override{
			assert(objlock.is_locked() && !objlock.is_exclusive());
			objlock.unlock();
		}
		bool is_locked(void) const override{
			return objlock.is_locked();
		}
		bool is_exclusive(void) const override{
			return objlock.is_exclusive();
		}
	};
	class user_vspace : public virtual_space{
		rwlock objlock;
		const qword cr3;
		const qword pl4te;

		static bool common_check(qword addr,dword page_count);
	public:
		user_vspace(void);
		~user_vspace(void);
		qword get_cr3(void) const override;
		qword reserve(qword addr,dword page_count) override;
		bool commit(qword addr,dword page_count) override;
		bool protect(qword addr,dword page_count,qword attrib) override;
		bool release(qword addr,dword page_count) override;

		dword write(qword va,const void* data,dword length) override;
		dword read(qword va,void* buffer,dword length) override;
		dword zero(qword va,dword length) override;

		PTE peek(qword va) override;
		bool try_lock(void) override{
			return objlock.try_lock(rwlock::SHARED);
		}
		void lock(void) override{
			objlock.lock(rwlock::SHARED);
		}
		void unlock(void) override{
			assert(objlock.is_locked() && !objlock.is_exclusive());
			objlock.unlock();
		}
		bool is_locked(void) const override{
			return objlock.is_locked();
		}
		bool is_exclusive(void) const override{
			return objlock.is_exclusive();
		}
	};
	extern kernel_vspace vm;
}
