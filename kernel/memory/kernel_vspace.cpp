#include "vm.hpp"
#include "pm.hpp"
#include "constant.hpp"
#include "util.hpp"
#include "lang.hpp"
#include "assert.hpp"
#include "lock_guard.hpp"
#include "sysinfo.hpp"

using namespace UOS;

static PDPT* const pdpt_table = (PDPT *)HIGHADDR(PDPT8_PBASE);

kernel_vspace::kernel_vspace(void){
	assert(pe_kernel);
	auto module_base = pe_kernel->imgbase;
	auto stk_commit = pe_kernel->stk_commit;
	auto stk_reserve = pe_kernel->stk_reserve;
	assert(module_base >= HIGHADDR(0x01000000) && stk_commit <= stk_reserve && stk_reserve <= 0x100*PAGE_SIZE);
	PDT* pdt_table = (PDT*)HIGHADDR(PDT0_PBASE);
	PT* pt0_table = (PT*)HIGHADDR(PT0_PBASE);

	stk_reserve = align_up(stk_reserve,PAGE_SIZE) >> 12;
	stk_commit = align_up(stk_commit,PAGE_SIZE) >> 12;
	auto stk_top = LOWADDR(KRNL_STK_TOP) >> 12;
	assert(stk_top < 0x200);

	dbgprint("kernel stack committed : %p - %p",HIGHADDR((stk_top - stk_commit)*PAGE_SIZE),HIGHADDR(stk_top*PAGE_SIZE));
	dbgprint("kernel stack reserved : %p - %p",HIGHADDR((stk_top - stk_reserve)*PAGE_SIZE),HIGHADDR((stk_top - stk_commit)*PAGE_SIZE));

	//stack guard
	assert(!pt0_table[stk_top].present);
	pt0_table[stk_top].preserve = 1;
	pt0_table[stk_top].bypass = 1;

	for (unsigned i = stk_top - stk_reserve;i < (stk_top - stk_commit);++i){
		assert(!pt0_table[i].present);
		pt0_table[i].preserve = 1;
	}
	//preserve & bypass boot area
	for (unsigned i = 0;i < (DIRECT_MAP_TOP >> 12);++i){
		assert(pt0_table[i].present);
		pt0_table[i].bypass = 1;
	}
	for (unsigned i = (DIRECT_MAP_TOP >> 12);i < (BOOT_AREA_TOP >> 12);++i){
		assert(pt0_table[i].present);
		pt0_table[i].present = 0;
		pt0_table[i].preserve = 1;
		pt0_table[i].bypass = 1;
		invlpg((void*)HIGHADDR(i*PAGE_SIZE));
	}
	BLOCK block;
	block.self = (BOOT_AREA_TOP >> 12);
	block.size = stk_top - block.self - stk_reserve;
	block.prev_valid = block.next_valid = 0;

	dbgprint("boot area free block : 0x%x - 0x%x",block.self, block.self + block.size);

	block.put(pt0_table + block.self);
	pdt_table[0].head = block.self;
	put_max_size(pdt_table[0],block.size);
	//MAP bypass
	constexpr auto map_off = LOWADDR(MAP_VIEW_BASE) >> 21;
	assert(pdt_table[map_off].present);
	pdt_table[map_off].bypass = 1;
	//PMMBMP bypass
	constexpr auto pdt_off = LOWADDR(PMMBMP_BASE) >> 21;
	auto pdt_count = align_up(pm.bmp_page_count(),0x200) >> 9;
	for (unsigned i = 0;i < pdt_count;++i){
		assert(pdt_table[pdt_off + i].present);
		pdt_table[pdt_off + i].bypass = 1;
	}
	//kernel bypass
	auto krnl_index = LOWADDR(module_base) >> 21;
	assert(krnl_index < 0x200 && pdt_table[krnl_index].present);
	pdt_table[krnl_index].bypass = 1;
	used_pages = pm.capacity() - pm.available();
	features.set(decltype(features)::MEM);
	int_trap(3);
}

qword kernel_vspace::get_cr3(void) const{
	return PL4T_PBASE;
}

bool kernel_vspace::common_check(qword addr,dword page_count){
	if (addr && page_count)
		;
	else
		return false;
	
	if ((addr & PAGE_MASK) || !IS_HIGHADDR(addr))
		return false;

	if (LOWADDR(addr + page_count*PAGE_SIZE) > size_512G){
		//over 512G not supported
		return false;
	}
	return true;
}

qword kernel_vspace::reserve(qword addr,dword page_count){
	if (addr){
		if (!common_check(addr,page_count))
			return 0;
	}
	else{
		if (!page_count || page_count > 0x40000){
			//over 1G not supported
			return 0;
		}
	}
	interrupt_guard<spin_lock> guard(objlock);
	if (addr){
		return reserve_fixed(pdpt_table,addr,page_count) ? addr : 0;
	}
	else{
		addr = HIGHADDR( (page_count < 0x200) ? \
			reserve_any(pdpt_table,page_count) : \
			reserve_big(pdpt_table,page_count) );
		
		assert(common_check(addr,page_count));
		return addr;
	}
}

bool kernel_vspace::release(qword addr,dword page_count){
	if (!common_check(addr,page_count))
		return false;
	interrupt_guard<spin_lock> guard(objlock);
	auto res = imp_iterate(pdpt_table,addr,page_count,[](PT& pt,qword,qword) -> bool{
		if (pt.bypass)
			return false;
		if (pt.present){
			return (pt.page_addr && !pt.user);
		}
		return pt.preserve;
	});
	if (res != page_count)
		return false;
	safe_release(pdpt_table,addr,page_count);
	return true;
}

bool kernel_vspace::commit(qword base_addr,dword page_count){
	if (!common_check(base_addr,page_count))
		return false;
	auto avl_count = pm.available();
	if (page_count >= avl_count){
		//no physical memory
		return false;
	}
	interrupt_guard<spin_lock> guard(objlock);
	auto res = imp_iterate(pdpt_table,base_addr,page_count,[](PT& pt,qword,qword) -> bool{
		return (pt.preserve && !pt.bypass && !pt.present);
	});
	if (res != page_count)
		return false;
	res = pm.reserve(page_count);
	if (!res)
		return false;
	res = imp_iterate(pdpt_table,base_addr,page_count,[](PT& pt,qword,qword) -> bool{
		assert(pt.preserve && !pt.bypass && !pt.present);
		pt.page_addr = pm.allocate(PM::TAKE) >> 12;
		pt.xd = 1;
		pt.pat = 0;
		pt.user = 0;
		pt.write = 1;
		pt.present = 1;
		return true;
	});
	if (res != page_count)
		bugcheck("page count mispatch (%x,%x)",res,page_count);
	used_pages += page_count;
	return true;
}

bool kernel_vspace::protect(qword base_addr,dword page_count,qword attrib){
	if (!common_check(base_addr,page_count))
		return false;
	//check for valid attrib
	qword mask = PAGE_XD | PAGE_GLOBAL | PAGE_CD | PAGE_WT | PAGE_WRITE;
	if (attrib & ~mask)
		return false;
	interrupt_guard<spin_lock> guard(objlock);
	auto res = imp_iterate(pdpt_table,base_addr,page_count,[](PT& pt,qword,qword) -> bool{
		return (pt.present && !pt.bypass && !pt.user && pt.page_addr);
	});
	if (res != page_count)
		return false;

	PTE_CALLBACK fun = [](PT& pt,qword addr,qword attrib) -> bool{
		assert(pt.present && !pt.bypass && !pt.user && pt.page_addr);
		pt.xd = (attrib & PAGE_XD) ? 1 : 0;
		pt.global = (attrib & PAGE_GLOBAL) ? 1 : 0;
		pt.cd = (attrib & PAGE_CD) ? 1 : 0;
		pt.wt = (attrib & PAGE_WT) ? 1 : 0;
		pt.write = (attrib & PAGE_WRITE) ? 1 : 0;
		invlpg((void*)addr);
		return true;
	};
	res = imp_iterate(pdpt_table,base_addr,page_count,fun,attrib);
	if (res != page_count)
		bugcheck("page count mispatch (%x,%x)",res,page_count);
	return true;
}

bool kernel_vspace::assign(qword base_addr,qword phy_addr,dword page_count){
	if (!common_check(base_addr,page_count) || !phy_addr)
		return false;
	interrupt_guard<spin_lock> guard(objlock);
	auto res = imp_iterate(pdpt_table,base_addr,page_count,[](PT& pt,qword,qword){
		return (pt.preserve && !pt.present) ? true : false;
	});
	if (res != page_count)
		return false;
	//assume phy_addr is far lower than base_addr
	//use delta to calc back phy_addr
	if (base_addr < phy_addr)
		bugcheck("assume phy_addr is far lower than base_addr (%x,%x)",phy_addr,base_addr);
	
	PTE_CALLBACK fun = [](PT& pt,qword addr,qword delta) -> bool{
		assert(pt.preserve && !pt.present);
		assert(addr >= delta);
		pt.page_addr = (addr - delta) >> 12;
		pt.xd = 1;
		pt.bypass = 1;
		pt.global = 1;
		pt.pat = 0;
		pt.cd = 1;
		pt.wt = 1;
		pt.user = 0;
		pt.write = 1;
		pt.present = 1;
		return true;
	};
	res = imp_iterate(pdpt_table,base_addr,page_count,fun,base_addr - phy_addr);
	if (res != page_count)
		bugcheck("page count mispatch (%x,%x)",res,page_count);
	return true;
}

PT kernel_vspace::peek(qword va){
	if (!IS_HIGHADDR(va) || LOWADDR(va) >= size_512G)
		return PT{0};
	return imp_peek(va,pdpt_table);
}