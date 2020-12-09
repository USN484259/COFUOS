#include "vm.hpp"
#include "pm.hpp"
#include "constant.hpp"
#include "util.hpp"
#include "lang.hpp"
#include "assert.hpp"
#include "lock_guard.hpp"
#include "../exception/include/kdb.hpp"

using namespace UOS;

static auto const pdpt_table = (VM::PDPT *)HIGHADDR(PDPT8_PBASE);

VM::kernel_vspace::kernel_vspace(void){
	assert(pe_kernel);
	auto module_base = pe_kernel->imgbase;
	auto stk_commit = pe_kernel->stk_commit;
	auto stk_reserve = pe_kernel->stk_reserve;
	assert(module_base >= HIGHADDR(0x00C00000) && stk_commit <= stk_reserve && stk_reserve <= 0x100*PAGE_SIZE);
	PDT* pdt_table = (PDT*)HIGHADDR(PDT0_PBASE);
	PT* pt0_table = (PT*)HIGHADDR(PT0_PBASE);

	stk_reserve = align_up(stk_reserve,PAGE_SIZE) >> 12;
	stk_commit = align_up(stk_commit,PAGE_SIZE) >> 12;
#ifdef VM_TEST
	dbgprint("stack committed : %p - %p",HIGHADDR((0x200 - stk_commit)*PAGE_SIZE),HIGHADDR(0x200*PAGE_SIZE));
	dbgprint("stack reserved : %p - %p",HIGHADDR((0x200 - stk_reserve)*PAGE_SIZE),HIGHADDR((0x200 - stk_commit)*PAGE_SIZE));
#endif
	for (unsigned i = 0x200 - stk_reserve;i < (0x200 - stk_commit);++i){
		assert(!pt0_table[i].present);
		pt0_table[i].preserve = 1;
	}
	//preserve & bypass boot area
	for (unsigned i = 0;i < 0x0A;++i){
		assert(pt0_table[i].present);
		pt0_table[i].bypass = 1;
	}
	for (unsigned i = 0x0A;i < 0x10;++i){
		assert(pt0_table[i].present);
		pt0_table[i].present = 0;
		pt0_table[i].preserve = 1;
		pt0_table[i].bypass = 1;
		__invlpg((void*)HIGHADDR(i*PAGE_SIZE));
	}
	BLOCK block;
	block.self = 0x10;
	block.size = 0x1F0 - stk_reserve;
	block.prev_valid = block.next_valid = 0;
#ifdef VM_TEST
	dbgprint("free block : 0x10 - 0x%x",block.self + block.size);
#endif
	block.put(pt0_table + block.self);
	pdt_table[0].head = block.self;
	put_max_size(pdt_table[0],block.size);
	//PMMBMP bypass
	assert(pdt_table[1].present);
	pdt_table[1].bypass = 1;
	//kernel bypass
	auto krnl_index = LOWADDR(module_base) >> 21;
	assert(krnl_index < 0x200 && pdt_table[krnl_index].present);
	pdt_table[krnl_index].bypass = 1;
}

void VM::kernel_vspace::new_pdt(PDPT& pdpt,map_view& view){
	assert(!pdpt.present);
	auto phy_addr = pm.allocate();
	view.map(phy_addr);
	PDT* table = (PDT*)view;
	zeromemory(table,PAGE_SIZE);

	pdpt = {0};
	pdpt.pdt_addr = phy_addr >> 12;
	pdpt.write = 1;
	pdpt.present = 1;
}

void VM::kernel_vspace::new_pt(VM::PDT& pdt,map_view& view){
	assert(!pdt.present && !pdt.bypass);
	auto phy_addr = pm.allocate();
	view.map(phy_addr);
	PT* table = (PT*)view;
	zeromemory(table,PAGE_SIZE);

	pdt = {0};
	pdt.pt_addr = phy_addr >> 12;
	pdt.write = 1;
	put_max_size(pdt,0x200);
	pdt.present = 1;

	BLOCK block = {0};
	block.size = 0x200;
	block.put(table);
}

bool VM::kernel_vspace::common_check(qword addr,size_t page_count){
	if (addr && page_count)
		;
	else
		return false;
	
	if ((addr & PAGE_MASK) || !IS_HIGHADDR(addr))
		return false;

	if (LOWADDR(addr + page_count*PAGE_SIZE) > 0x008000000000){
		//over 512G not supported
		return false;
	}
	return true;
}

qword VM::kernel_vspace::reserve(qword addr,size_t page_count){
	if (!page_count)
		return 0;
	if (addr){
		if (!common_check(addr,page_count))
			return 0;
	}
	else{
		if (page_count > 0x200){
			//TODO allocate large pages
			//not implemented
			return 0;
		}
	}
	
	lock_guard<spin_lock> guard(lock);
	if (addr){
		return reserve_fixed(addr,page_count) ? addr : 0;
	}
	else{
		return reserve_any(page_count);
	}

}

qword VM::kernel_vspace::reserve_any(size_t page_count){
	assert(lock.is_locked());
	map_view pdt_view;
	map_view pt_view;
	for (unsigned pdpt_index = 0;pdpt_index < 0x200;++pdpt_index){
		if (!pdpt_table[pdpt_index].present){   //allocate new PDT
			new_pdt(pdpt_table[pdpt_index],pdt_view);
		}
		else{
			pdt_view.map(pdpt_table[pdpt_index].pdt_addr << 12);
		}
		PDT* pdt_table = (PDT*)pdt_view;
		unsigned i;
		for (i = 0;i < 0x200;++i){
			auto& cur = pdt_table[i];
			if (cur.bypass)
				continue;
			if (!cur.present){  //allocate new PT
				new_pt(cur,pt_view);
			}
			else{
				pt_view.map(cur.pt_addr << 12);
			}
			PT* table = (PT*)pt_view;
			qword base_addr = HIGHADDR(\
				pdpt_index*0x40000000 \
				+ i * 0x200000 \
			);
			auto res = imp_reserve_any(cur,table,base_addr,page_count);
			if (res)
				return res;
		}
	}
	return 0;
}

bool VM::kernel_vspace::reserve_fixed(qword base_addr,size_t page_count){
	assert(lock.is_locked());
	assert(base_addr && page_count);
	assert(0 == (base_addr & PAGE_MASK));

	map_view pdt_view;
	map_view pt_view;

	auto pdpt_index = (base_addr >> 30) & 0x1FF;
	auto pdt_index = (base_addr >> 21) & 0x1FF;
	auto addr = base_addr;
	size_t count = 0;
	while (count < page_count){
		if (!pdpt_table[pdpt_index].present){
			new_pdt(pdpt_table[pdpt_index],pdt_view);
		}
		else{
			pdt_view.map(pdpt_table[pdpt_index].pdt_addr << 12);
		}
		PDT* pdt_table = (PDT*)pdt_view;
		while(count < page_count){
			auto& cur = pdt_table[pdt_index];
			if (cur.bypass){
				goto rollback;
			}
			if (!cur.present){
				new_pt(cur,pt_view);
			}
			else{
				pt_view.map(cur.pt_addr << 12);
			}
			PT* table = (PT*)pt_view;
			auto off = (addr >> 12) & 0x1FF;
			auto size = min(page_count - count,0x200 - off);

			auto res = imp_reserve_fixed(cur,table,off,size);
			if (!res){
				goto rollback;
			}
			addr += PAGE_SIZE*size;
			count += size;
			if (++pdt_index == 0x200){
				pdt_index = 0;
				break;
			}
		}
		++pdpt_index;
		assert(pdpt_index < 0x200);
	}
	assert(count == page_count);
	return true;
rollback:
	assert(count < page_count);
	if (count)
		locked_release(base_addr,count);
	return false;
}

bool VM::kernel_vspace::release(qword addr,size_t page_count){
	if (!common_check(addr,page_count))
		return false;
	lock_guard<spin_lock> guard(lock);
	auto res = imp_iterate(pdpt_table,addr,page_count,[](PT& pt,qword,qword) -> bool{
		if (pt.bypass)
			return false;
		if (pt.present){
			return (pt.page_addr && !pt.user) ? true : false;
		}
		return pt.preserve ? true : false;
	});
	if (res != page_count)
		return false;
	locked_release(addr,page_count);
	return true;
}

void VM::kernel_vspace::locked_release(qword base_addr,size_t page_count){
	assert(lock.is_locked());
	assert(base_addr && page_count);
	assert(0 == (base_addr & PAGE_MASK));

	map_view pdt_view;
	map_view pt_view;

	auto pdpt_index = (base_addr >> 30) & 0x1FF;
	auto pdt_index = (base_addr >> 21) & 0x1FF;
	auto addr = base_addr;
	size_t count = 0;
	while(count < page_count){
		if (!pdpt_table[pdpt_index].present){
			BugCheck(corrupted,pdpt_table + pdpt_index);
		}
		pdt_view.map(pdpt_table[pdpt_index].pdt_addr << 12);
		PDT* pdt_table = (PDT*)pdt_view;
		while(count < page_count){
			auto& cur = pdt_table[pdt_index];
			if (cur.bypass || cur.user || !cur.present){
				BugCheck(corrupted,cur);
			}
			pt_view.map(cur.pt_addr << 12);
			PT* table = (PT*)pt_view;
			auto off = (addr >> 12) & 0x1FF;
			auto size = min(page_count - count,0x200 - off);

			imp_release(cur,table,addr,size);

			addr += PAGE_SIZE*size;
			count += size;
			if (++pdt_index == 0x200){
				pdt_index = 0;
				break;
			}
		}
		++pdpt_index;
		assert(pdpt_index < 0x200);
	}
	assert(count == page_count);
}

bool VM::kernel_vspace::commit(qword base_addr,size_t page_count){
	if (!common_check(base_addr,page_count))
		return false;
	auto avl_count = pm.available();
	if (page_count >= avl_count){
		//no physical memory
		return false;
	}
	lock_guard<spin_lock> guard(lock);
	auto res = imp_iterate(pdpt_table,base_addr,page_count,[](PT& pt,qword,qword) -> bool{
		return (pt.preserve && !pt.bypass && !pt.present) ? true : false;
	});
	if (res != page_count)
		return false;

	res = imp_iterate(pdpt_table,base_addr,page_count,[](PT& pt,qword,qword) -> bool{
		assert(pt.preserve && !pt.bypass && !pt.present);
		pt.page_addr = pm.allocate() >> 12;
		pt.xd = 1;
		pt.write = 1;
		pt.user = 0;
		pt.present = 1;
		return true;
	});
	if (res != page_count)
		BugCheck(corrupted,res);
	return true;
}

bool VM::kernel_vspace::protect(qword base_addr,size_t page_count,qword attrib){
	if (!common_check(base_addr,page_count))
		return false;
	//check for valid attrib
	qword mask = PAGE_XD | PAGE_GLOBAL | PAGE_CD | PAGE_WT | PAGE_WRITE;
	if (attrib & ~mask)
		return false;

	lock_guard<spin_lock> guard(lock);
	auto res = imp_iterate(pdpt_table,base_addr,page_count,[](PT& pt,qword,qword) -> bool{
		return (pt.present && !pt.bypass && !pt.user && pt.page_addr) ? true : false;
	});
	if (res != page_count)
		return false;

	static const auto fun = [](PT& pt,qword addr,qword attrib) -> bool{
		assert(pt.present && !pt.bypass && !pt.user && pt.page_addr);
		pt.xd = (attrib & PAGE_XD) ? 1 : 0;
		pt.global = (attrib & PAGE_GLOBAL) ? 1 : 0;
		pt.cd = (attrib & PAGE_CD) ? 1 : 0;
		pt.wt = (attrib & PAGE_WT) ? 1 : 0;
		pt.write = (attrib & PAGE_WRITE) ? 1 : 0;
		__invlpg((void*)addr);
		return true;
	};
	res = imp_iterate(pdpt_table,base_addr,page_count,fun,attrib);
	if (res != page_count)
		BugCheck(corrupted,res);
	return true;
}

bool VM::kernel_vspace::peek(void*,qword,size_t){
	//TODO
	return false;
}