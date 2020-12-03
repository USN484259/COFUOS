#include "pm.hpp"
#include "sysinfo.hpp"
#include "constant.hpp"
#include "util.hpp"
#include "assert.hpp"
#include "lock_guard.hpp"
#include "vm.hpp"
#include "lang.hpp"
#include "../exception/include/kdb.hpp"

using namespace UOS;

struct PMMSCAN{
	qword base;
	qword len;
	dword type;
	dword reserved;
};

struct BLOCK{
	byte tag	:	6;
	byte solid	:	1;		//0	-> pierce
	byte free	:	1;		//0 -> used
};
static_assert(sizeof(BLOCK) == sizeof(byte),"BLOCK size mismatch");

PM::PM(void) : bmp_size(0),total(0),used(0),next(0){
	auto scan_base = (PMMSCAN*)PMMSCAN_BASE;
	bmp_size = sysinfo->PMM_avl_top >> 12;

	qword page_count = align_up(bmp_size,PAGE_SIZE) >> 12;
	qword pt_count = align_up(page_count,PAGE_SIZE/sizeof(qword)) >> 9;

	dbgprint("bmp_size = %x, page_count = %x, pt_count = %x",bmp_size,page_count,pt_count);

	if (pt_count > 6)
		BugCheck(not_implemented,bmp_size);
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
		BugCheck(bad_alloc,bmp_size);
	
	//maps everything, unmap PT
	auto pdt0 = (volatile qword*)HIGHADDR(PDT0_PBASE);
	qword i;
	for (i = 0;i < pt_count;++i){
		qword data = 0x8000000000000103 | (PMMBMP_PT_PBASE + i*PAGE_SIZE);
		assert(pdt0[2 + i] == 0);
		pdt0[2 + i] = data;
	}
	auto bmp_pt = (volatile qword*)HIGHADDR(PMMBMP_PT_PBASE);
	for (i = 0;i < page_count;++i){
		assert(i < 6*PAGE_SIZE/sizeof(qword));
		qword data = 0x8000000000000103 | (bmp_pbase + i*PAGE_SIZE);
		bmp_pt[i] = data;
	}
	while(i < pt_count*PAGE_SIZE/sizeof(qword)){
		assert(i < 6*PAGE_SIZE/sizeof(qword));
		bmp_pt[i] = 0;
		++i;
	}
	auto pt0 = (volatile qword*)HIGHADDR(PT0_PBASE);
	for (i = 0x0A;i < 0x10;++i){
		assert(pt0[i] & 0x01);
		pt0[i] = 0;
		__invlpg((void*)HIGHADDR(i*PAGE_SIZE));
	}

	auto pmm_bmp = (BLOCK*)PMMBMP_BASE;
	zeromemory(pmm_bmp,page_count*PAGE_SIZE);
	//scan & fill wmp
	for (auto it = scan_base;it->type;++it){
		dbgprint("%p ==> %p : %x",it->base,it->base+it->len,it->type);

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
	//set pre-allocated pages
	//auto pre_alloc_size = 0x0B + sysinfo->kernel_page;
	for (i = 0;i < 0x0A + pt_count;++i){
		pmm_bmp[i].free = 0;
		pmm_bmp[i].tag = 0x3F;
	}

	for (i = 0;i < sysinfo->kernel_page;++i){
		assert(0x10 + i < bmp_size);
		pmm_bmp[0x10 + i].free = 0;
		pmm_bmp[0x10 + i].tag = 0x3F;
	}
	for (i = 0;i < page_count;++i){
		assert((bmp_pbase >> 12) + i < bmp_size);
		pmm_bmp[(bmp_pbase >> 12) + i].free = 0;
		pmm_bmp[(bmp_pbase >> 12) + i].tag = 0x3F;
	}
	used = 0x0A + pt_count + page_count + sysinfo->kernel_page;
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
}

qword PM::allocate(byte tag){
	lock_guard<spin_lock> guard(lock);
	auto pmm_bmp = (BLOCK*)PMMBMP_BASE;
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
			assert(used == total);
			return 0;
		}
	}while (true);
	next = page;
	dbgprint("page_count = %x, selected page = %x",page_count,page);

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
	dbgprint("selected %x",head);
	qword res_page = page*PAGE_SIZE + head;
	assert(res_page < bmp_size);

	auto& res_stat = pmm_bmp[res_page];
	assert(res_stat.solid && res_stat.free);
	res_stat.free = 0;
	res_stat.tag = tag;

	while(head){
		--head;
		auto& cur = pmm_bmp[page* PAGE_SIZE + head];
		assert(0 == cur.solid);
		cur.solid = 1;
		if (cur.free)
			break;
	}
	assert(used < total);
	++used;
#ifdef PM_TEST
	check_integration();
#endif
	return res_page << 12;
}

void PM::release(qword pa){
	assert(0 == (pa & PAGE_MASK));
	auto page = pa >> 12;
	assert(page < bmp_size);
	lock_guard<spin_lock> guard(lock);
	auto pmm_bmp = (BLOCK*)PMMBMP_BASE;
	auto& cur = pmm_bmp[page];
	if (cur.free)
		BugCheck(corrupted,pa);
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
	check_integration();
#endif
}

qword PM::capacity(void) const{
	return total;
}

qword PM::available(void) const{
	assert(total >= used);
	return total - used;
}

bool PM::peek(void* dest,qword paddr,size_t count){
	qword off = paddr & PAGE_MASK;
	qword page = paddr - off;
	while(count){
		auto view = (byte*)VM::map_view(page);
		qword len = min(PAGE_SIZE - off, count);
		memcpy(dest, view + off, len);
		count -= len;
		dest = (byte*)dest + len;
		off = 0;
		page += PAGE_SIZE;
		VM::unmap_view(view);
	}
	return true;
}

#ifdef PM_TEST
void PM::check_integration(void){
	auto pmm_bmp = (BLOCK*)PMMBMP_BASE;
	auto page_count = align_up(bmp_size,PAGE_SIZE) >> 12;
	auto discovered_free_page = 0;
	for (auto page = 0;page < page_count;++page){
		auto pierce = 1;
		for (auto i = 0;i < PAGE_SIZE;++i){
			auto& cur = pmm_bmp[page*PAGE_SIZE + i];
			if (pierce){
				if (cur.solid){
					if (cur.free || i == 0)
						pierce = 0;
					else
						BugCheck(corrupted,cur);
				}
				if (cur.free)
					++discovered_free_page;
			}
			else if (1 == cur.free || 0 == cur.solid)
				BugCheck(corrupted,cur);
		}
		if (pierce)
			BugCheck(corrupted,pierce);
	}
	if (discovered_free_page + used != total)
		BugCheck(corrupted,discovered_free_page);
}
#endif