#include "pm.hpp"
#include "sysinfo.hpp"
#include "vm.hpp"
#include "lang.hpp"
#include "lock_guard.hpp"

using namespace UOS;

struct PMMSCAN{
	qword base;
	qword len;
	dword type;
	dword reserved;
};

struct BLOCK{
	byte 		:	6;
	byte solid	:	1;		//0	-> pierce
	byte free	:	1;		//0 -> used
};
static_assert(sizeof(BLOCK) == sizeof(byte),"BLOCK size mismatch");

PM::PM(void) : bmp_size(0),total(0),used(0),next(0){
	PMMSCAN* const scan_base = (PMMSCAN*)PMMSCAN_BASE;
	bmp_size = sysinfo->PMM_avl_top >> 12;

	qword page_count = align_up(bmp_size,PAGE_SIZE) >> 12;
	qword pdt_count = align_up(page_count,PAGE_SIZE/sizeof(qword)) >> 9;

	dbgprint("bmp_size = %x, page_count = %x, pdt_count = %x",bmp_size,page_count,pdt_count);

	constexpr auto pdt_limit = (BOOT_AREA_TOP - DIRECT_MAP_TOP) >> 12;

	if (pdt_count > pdt_limit)
		bugcheck("too many physical memory (0x%x pages)",bmp_size);
	//find pbase for bmp
	qword bmp_pbase = 0;
	for (auto it = scan_base;it->type;++it){
		if (it->base < 0x100000)	//skip 1MB area
			continue;
		if (it->type != 1)	//not usable
			continue;
		qword aligned_base = align_up(it->base,PAGE_SIZE);
		if (it->base + it->len < aligned_base)
			continue;
		qword aligned_size = align_down(it->base + it->len - aligned_base,PAGE_SIZE);
		if (aligned_size < page_count*PAGE_SIZE)
			continue;
		
		bmp_pbase = aligned_base;
		break;
	}

	dbgprint("pmmbmp_pbase = %p",bmp_pbase);

	if (0 == bmp_pbase)
		bugcheck("cannot allocate bmp_pbase");
	
	//maps everything
	auto pdt0 = (qword volatile* const)HIGHADDR(PDT0_PBASE);
	constexpr auto pdt_off = LOWADDR(PMMBMP_BASE) >> 21;
	qword i;
	for (i = 0;i < pdt_count;++i){
		qword data = 0x8000000000000103 | (PMMBMP_PT_PBASE + i*PAGE_SIZE);
		assert(pdt0[pdt_off + i] == 0);
		pdt0[pdt_off + i] = data;
	}
	auto bmp_pt = (volatile qword*)HIGHADDR(PMMBMP_PT_PBASE);
	for (i = 0;i < page_count;++i){
		assert(i < pdt_limit*PAGE_SIZE/sizeof(qword));
		qword data = 0x8000000000000103 | (bmp_pbase + i*PAGE_SIZE);
		bmp_pt[i] = data;
	}
	while(i < pdt_count*PAGE_SIZE/sizeof(qword)){
		assert(i < pdt_limit*PAGE_SIZE/sizeof(qword));
		bmp_pt[i] = 0;
		++i;
	}

	auto pmm_bmp = (BLOCK* const)PMMBMP_BASE;
	zeromemory(pmm_bmp,page_count*PAGE_SIZE);
	//scan & fill wmp
	for (auto it = scan_base;it->type;++it){
		dbgprint("%p ==> %p : %d",it->base,it->base+it->len,it->type);

		if (it->type != 1 || it->len == 0)
			continue;
		qword aligned_base = align_up(it->base,PAGE_SIZE);
		if (it->base + it->len < aligned_base)
			continue;
		qword aligned_count = align_down(it->base + it->len - aligned_base,PAGE_SIZE) >> 12;

		dbgprint("aligned to %p ==> %p",aligned_base,aligned_base + PAGE_SIZE*aligned_count);
		auto index = aligned_base / PAGE_SIZE;
		while(aligned_count--){
			assert(index < bmp_size);
			if (0 == pmm_bmp[index].free){
				++total;
				pmm_bmp[index].free = 1;
			}
			++index;
		}
	}
	constexpr auto direct_map_count = DIRECT_MAP_TOP >> 12;
	constexpr auto boot_area_count = BOOT_AREA_TOP >> 12;
	//set pre-allocated pages
	//auto pre_alloc_size = 0x0B + sysinfo->kernel_page;
	for (i = 0;i < direct_map_count + pdt_count;++i){
		pmm_bmp[i].free = 0;
		//pmm_bmp[i].tag = 0x3F;
	}

	for (i = 0;i < sysinfo->kernel_page;++i){
		assert(boot_area_count + i < bmp_size);
		pmm_bmp[boot_area_count + i].free = 0;
		//pmm_bmp[boot_area_count + i].tag = 0x3F;
	}
	for (i = 0;i < page_count;++i){
		assert((bmp_pbase >> 12) + i < bmp_size);
		pmm_bmp[(bmp_pbase >> 12) + i].free = 0;
		//pmm_bmp[(bmp_pbase >> 12) + i].tag = 0x3F;
	}
	used = direct_map_count + pdt_count + page_count + sysinfo->kernel_page;
	assert(used < total);

	//build solid
	for (i = 0;i < page_count;++i){
		for (auto off = PAGE_SIZE;off;){
			--off;
			auto& cur = pmm_bmp[i*PAGE_SIZE + off];
			cur.solid = 1;
			if (cur.free)
				break;
		}
	}

	soft_critical_limit = min<dword>(max<dword>(total/64,0x10),0x1000);	// [ 64K -- (limit) -- 16M ]
	hard_critical_limit = 0;
	reserved = 0;
	callback = nullptr;
}

void PM::set_mp_count(dword count){
	interrupt_guard<spin_lock> guard(lock);
	if (hard_critical_limit)
		bugcheck("invalid PM::set_mp_count call from %p",return_address());
	hard_critical_limit = 4*count;
}

void PM::set_critical_callback(critical_callback cb,void* ud){
	interrupt_guard<spin_lock> guard(lock);
	if (callback)
		bugcheck("invalid PM::set_critical_callback call from %p",return_address());
	callback = cb;
	userdata = ud;
}

void PM::critical_check(void){
	if (used + reserved + soft_critical_limit >= total){
		if (callback)
			callback(*this,userdata);
	}
}

qword PM::allocate(MODE mode){
	critical_check();
	interrupt_guard<spin_lock> guard(lock);
	if (mode == NONE && used + reserved + hard_critical_limit >= total){
		return 0;
	}

	auto pmm_bmp = (BLOCK* const)PMMBMP_BASE;
	auto page_count = align_up(bmp_size,PAGE_SIZE) >> 12;
	auto page = next;
	do{
		assert(page < page_count);
		auto& cur = pmm_bmp[page*PAGE_SIZE];
		if (1 == cur.solid && 0 == cur.free){
			;	//no free memory, next page
		}
		else{
			break;	//has free page
		}
		if (++page == page_count)
			page = 0;
		if (page == next){	//no free memory
			bugcheck("physical memory used up (%x,%x,%x)",total,used,reserved);
			//return 0;
		}
	}while (true);
	next = page;

	qword head = 0,tail = PAGE_SIZE;
	while (head + 1 < tail){
		auto mid = (head + tail)/2;
		auto& cur = pmm_bmp[page*PAGE_SIZE + mid];
		if (cur.solid){
			tail = mid;
		}
		else{
			head = mid;
		}
	}
	assert(head + 1 == tail);
	assert(tail < PAGE_SIZE);
	if (0 == pmm_bmp[page*PAGE_SIZE + head].solid)
		++head;
	assert(head < PAGE_SIZE);

	qword res_page = page*PAGE_SIZE + head;
	assert(res_page < bmp_size);

	auto& res_stat = pmm_bmp[res_page];
	assert(res_stat.solid && res_stat.free);
	res_stat.free = 0;

	while(head){
		--head;
		auto& cur = pmm_bmp[page* PAGE_SIZE + head];
		assert(0 == cur.solid);
		cur.solid = 1;
		if (cur.free)
			break;
	}
	assert(used + reserved < total);
	if (mode == TAKE){
		assert(reserved);
		--reserved;
	}
	++used;
#ifdef PM_TEST
	check_integrity();
#endif
	return res_page << 12;
}

bool PM::reserve(dword page_count){
	critical_check();
	interrupt_guard<spin_lock> guard(lock);
	if (used + reserved + page_count + hard_critical_limit < total){
		reserved += page_count;
		return true;
	}
	return false;
}

void PM::release(qword pa){
	assert(0 == (pa & PAGE_MASK));
	auto page = pa >> 12;
	assert(page < bmp_size);
	interrupt_guard<spin_lock> guard(lock);
	auto pmm_bmp = (BLOCK* const)PMMBMP_BASE;
	auto& cur = pmm_bmp[page];
	if (cur.free)
		bugcheck("double release %p",pa);
	cur.free = 1;
	if (page + 1 == PAGE_SIZE){		//last element
		assert(cur.solid);
	}
	else{	//solid state no change
		;
	}
	while(page & PAGE_MASK){	//clear solid of forw blocks
		--page;
		if (0 == pmm_bmp[page].solid)
			break;
		pmm_bmp[page].solid = 0;
	}
	assert(used);
	--used;
#ifdef PM_TEST
	check_integrity();
#endif
}

bool PM::peek(void* dest,qword paddr,size_t count){
	qword off = paddr & PAGE_MASK;
	qword page = paddr - off;
	map_view view;
	while(count){
		view.map(page);
		qword len = min(PAGE_SIZE - off, count);
		memcpy(dest, (byte*)view + off, len);
		count -= len;
		dest = (byte*)dest + len;
		off = 0;
		page += PAGE_SIZE;
	}
	return true;
}

#ifdef PM_TEST
void PM::check_integrity(void){
	auto pmm_bmp = (BLOCK* const)PMMBMP_BASE;
	auto page_count = align_up(bmp_size,PAGE_SIZE) >> 12;
	auto discovered_free_page = 0;
	for (unsigned page = 0;page < page_count;++page){
		auto pierce = 1;
		for (unsigned i = 0;i < PAGE_SIZE;++i){
			auto& cur = pmm_bmp[page*PAGE_SIZE + i];
			if (pierce){
				if (cur.solid){
					if (cur.free || i == 0)
						pierce = 0;
					else
						bugcheck("PM corrupted");
				}
				if (cur.free)
					++discovered_free_page;
			}
			else if (1 == cur.free || 0 == cur.solid)
				bugcheck("PM corrupted");
		}
		if (pierce)
			bugcheck("PM corrupted");
	}
	if (discovered_free_page + used != total)
		bugcheck("PM corrupted: free size mispatch 0x%x",discovered_free_page);
}
#endif