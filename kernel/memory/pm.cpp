#include "pm.hpp"
//#include "ps.hpp"
#include "sysinfo.hpp"
#include "constant.hpp"
#include "util.hpp"
#include "bits.hpp"
#include "assert.hpp"
//#include "mp.hpp"
#include "lock_guard.hpp"
#include "atomic.hpp"
#include "vm.hpp"

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

	qword page_count = align_up(bmp_size,PAGE_SIZE);
	qword pt_count = align_up(page_count,PAGE_SIZE);
	if (pt_count > 6)
		BugCheck(not_implemented,bmp_size);
	//find pbase for wmp
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
	auto wmp_pt = (volatile qword*)HIGHADDR(PMMBMP_PT_PBASE);
	for (i = 0;i < page_count;++i){
		assert(i < 6*PAGE_SIZE);
		qword data = 0x8000000000000103 | (bmp_pbase + i*PAGE_SIZE);
		wmp_pt[i] = data;
	}
	while(i < pt_count*PAGE_SIZE){
		wmp_pt[i] = 0;
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
		if (it->type != 1 || it->len == 0)
			continue;
		qword aligned_base = align_up(it->base,PAGE_SIZE);
		if (it->base + it->len < aligned_base)
			continue;
		qword aligned_size = align_down(it->base + it->len - aligned_base,PAGE_SIZE);
		if (aligned_size == 0)
			continue;
		auto index = aligned_base / PAGE_SIZE;
		while(aligned_size--){
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
	used = 0x0A + pt_count + page_count;
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
	auto page_count = align_up(bmp_size,PAGE_SIZE);
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

	qword head = 0,tail = PAGE_SIZE;
	while (head < tail){
		auto mid = (head + tail)/2;
		auto& cur = pmm_bmp[page*PAGE_SIZE + mid];
		if (cur.solid){
			tail = mid;
		}
		else{
			head = mid;
		}
	}
	assert(head < PAGE_SIZE);
	assert(head == tail);

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
	else{	//pass status of back solid
		cur.solid = pmm_bmp[page + 1].solid;
	}
	while(page & PAGE_MASK){	//clear solid of forw blocks
		--page;
		if (0 == pmm_bmp[page].solid)
			break;
		pmm_bmp[page].solid = 0;
	}
}

bool PM::peek(void* dest,qword paddr,size_t count){
	qword off = paddr & PAGE_MASK;
	qword page = paddr - off;
	while(count){
		VM::map_view view(page);
		qword len = min(PAGE_SIZE - off, count);
		memcpy(dest, view + off, len);
		count -= len;
		dest = (byte*)dest + len;
		off = 0;
		page += PAGE_SIZE;
	}
	return true;
}