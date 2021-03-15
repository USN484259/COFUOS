#include "vm.hpp"
#include "pm.hpp"
#include "constant.hpp"
#include "util.hpp"
#include "assert.hpp"
#include "intrinsics.hpp"

using namespace UOS;

map_view::map_view(qword pa,qword attrib){
	assert(view == nullptr);
	map(pa,attrib);
}
map_view::~map_view(void){
	unmap();
}

void map_view::map(qword pa,qword attrib){
	assert(0 == (pa & PAGE_MASK));
	assert(0 == (attrib & 0x7FFFFFFFFFFFF004));	//not user, attributes only
	unmap();
	auto table = (qword volatile* const)MAP_TABLE_BASE;
	for (unsigned index = 0; index < 0x200; ++index)
	{
		qword origin_value = table[index];
		if (origin_value & 0x01)
			continue;
		
		qword new_value = attrib | pa | PAGE_PRESENT;

		if (origin_value == cmpxchg(table + index, new_value, origin_value)){
			//gain this slot
			view = (void*)(MAP_VIEW_BASE + PAGE_SIZE * index);
			return;
		}
	}
	bugcheck("map_view::map failed");
}

void map_view::unmap(void){
	if (!view)
		return;
	qword addr = (qword)view;
	assert(0 == (addr & PAGE_MASK));
	if (addr < MAP_VIEW_BASE)
		bugcheck("out_of_range %p",view);
	auto index = (addr - MAP_VIEW_BASE) >> 12;
	if (index >= 0x200)
		bugcheck("out_of_range %p",view);

	auto table = (qword volatile* const)MAP_TABLE_BASE;
	qword origin_value = table[index];
	if (0 == (origin_value & 0x01))
		bugcheck("double free @ %x", index);
	if (origin_value != cmpxchg(table + index, (qword)0, origin_value))
		bugcheck("double free @ %x", index);
	invlpg(view);
}

void virtual_space::BLOCK::get(const PT* pt){
	assert(0 == ((qword)pt & 0x07));
	self = (word)(((qword)pt & PAGE_MASK) >> 3);
	assert(!pt->present && !pt->preserve);

	switch(pt->type){
		case PT::OFF:
			assert(pt->valid);
			assert(pt->data > 2 && pt->data <= self);
			self -= (word)pt->data;
			pt -= pt->data;
			break;
		case PT::SIZE:
			assert(pt->valid);
			assert(pt->data == 3 && self >= 2);
			self -= 2;
			pt -= 2;
			break;
		case PT::PREV:
			assert(self >= 1);
			self -= 1;
			pt -= 1;
			break;
		case PT::NEXT:
			break;
	}

	assert(!pt->present && !pt->preserve);
	assert(pt->type == PT::NEXT);
	next = pt->data;
	next_valid = pt->valid;
	prev_valid = 0;
	size = 1;

	++pt;
	if (0 == ((qword)pt & PAGE_MASK) || pt->present || pt->preserve){
		return;		//just one page
	}
	assert(pt->type == PT::PREV);
	prev = pt->data;
	prev_valid = pt->valid;
	size = 2;

	++pt;
	if (0 == ((qword)pt & PAGE_MASK) || pt->present || pt->preserve){
		return;		//just two pages
	}
	assert(pt->type == PT::SIZE && pt->valid);
	size = pt->data + 1;
	assert(size > 2);

#ifndef NDEBUG
	for (unsigned i = 3;i < size;++i) {
		++pt;
		assert((qword)pt & PAGE_MASK);
		assert(!pt->present && !pt->preserve);
		assert(pt->type == PT::OFF);
		assert(pt->valid && pt->data == i);
	}
#endif

}

void virtual_space::BLOCK::put(PT* pt) const{
	assert(0 == ((qword)pt & 0x07));
	assert(self == (((qword)pt & PAGE_MASK) >> 3));
	assert(size);
	assert(!pt->present && !pt->preserve);
	pt->type = PT::NEXT;
	pt->data = next;
	pt->valid = next_valid ? 1 : 0;

	if (size == 1)
		return;
	++pt;
	assert((qword)pt & PAGE_MASK);
	assert(!pt->present && !pt->preserve);
	pt->type = PT::PREV;
	pt->data = prev;
	pt->valid = prev_valid ? 1 : 0;

	if (size == 2)
		return;
	++pt;
	assert((qword)pt & PAGE_MASK);
	assert(!pt->present && !pt->preserve);
	pt->type = PT::SIZE;
	pt->data = size - 1;
	pt->valid = 1;

	for (unsigned i = 3;i < size;++i) {
		++pt;
		assert((qword)pt & PAGE_MASK);
		assert(!pt->present && !pt->preserve);
		pt->type = PT::OFF;
		pt->valid = 1;
		pt->data = i;
	}
#ifndef NDEBUG
	++pt;
	assert(0 == ((qword)pt & PAGE_MASK) || pt->present || pt->preserve);
#endif
}

void virtual_space::resolve_prior(const PDT& pdt,const PT* table,BLOCK& block){
	assert(block.size);
	assert(get_max_size(pdt));
	if (pdt.head == block.self){
		assert(!block.prev_valid);
		return;
	}
	if (block.size > 1){
		assert(block.prev_valid);
		return;
	}
	BLOCK prev_block;
	prev_block.next = pdt.head;
	do{
		prev_block.get(table + prev_block.next);
		assert(prev_block.size == 1);
		if (prev_block.next_valid && prev_block.next == block.self){
			block.prev = prev_block.self;
			block.prev_valid = 1;
			return;
		}
	}while(prev_block.next_valid);
	bugcheck("virtual_space::resolve_prior failed @ %p",&block);
}


word virtual_space::get_max_size(const PDT& pdt){
	word remain = pdt.remain;
	if (remain < 8)
		return remain ? (word)1 << (remain - 1) : (word)0;
	else
		return (word)(64 + 56*(remain - 7));
}

void virtual_space::put_max_size(PDT& pdt,word max_size){
	if (max_size >= 64 + 56)
		pdt.remain = (word)(7 + (max_size - 64) / 56);
	else{
		word remain = 0;
		while (max_size){
			max_size >>= 1;
			++remain;
		}
		pdt.remain = remain;
	}
}

void virtual_space::erase(PDT& pdt,PT* table,BLOCK& block){
	auto max_size = 0;
	if (block.prev_valid){
		BLOCK prev_block;
		prev_block.get(table + block.prev);
		assert(prev_block.next_valid && prev_block.next == block.self);
		prev_block.next = block.next;
		prev_block.next_valid = block.next_valid;
		prev_block.put(table + prev_block.self);
		max_size = prev_block.size;
	}
	else{
		assert(pdt.head == block.self);
		pdt.head = block.next;
	}

	if (block.next_valid){
		BLOCK next_block;
		next_block.get(table + block.next);
		if (next_block.size > 1){
			assert(next_block.prev_valid && next_block.prev == block.self);
			next_block.prev = block.prev;
			next_block.prev_valid = block.prev_valid;
			next_block.put(table + next_block.self);
		}
		max_size = max(next_block.size,get_max_size(pdt));
	}
	put_max_size(pdt,max_size);
}

void virtual_space::insert(PDT& pdt,PT* table,BLOCK& block){
	if (block.size == 0)
		return;
	BLOCK prev_block;
	BLOCK next_block;
	if (!block.prev_valid && !block.next_valid){
		//without link
		if (get_max_size(pdt) != 0){
			//head valid, put before head
			block.next = pdt.head;
			block.next_valid = 1;
		}
	}

	if (block.prev_valid){
		prev_block.get(table + block.prev);
	}
	else{
		prev_block.size = 0;
	}
	if (block.next_valid){
		next_block.get(table + block.next);
	}
	else{
		next_block.size = 0;
	}
	auto func = (prev_block.size && prev_block.size > block.size) ? \
		[](PT* table,BLOCK& block,BLOCK& prev_block,BLOCK& next_block) -> bool{
			//shift left
			if (prev_block.size && prev_block.size > block.size)
				;
			else
				return false;
			next_block = prev_block;
			if (!next_block.prev_valid){
				prev_block.size = 0;
				return false;
			}
			prev_block.get(table + next_block.prev);
			return true;
		} : \
		[](PT* table,BLOCK& block,BLOCK& prev_block,BLOCK& next_block) -> bool{
			//shift right
			if (next_block.size && next_block.size < block.size)
				;
			else
				return false;
			prev_block = next_block;
			if (!prev_block.next_valid){
				next_block.size = 0;
				return false;
			}
			next_block.get(table + prev_block.next);
			return true;
		};
	do{
		if (prev_block.size && next_block.size)
			assert(prev_block.size <= next_block.size);
	}while(func(table,block,prev_block,next_block));
	if (next_block.size){
		assert(next_block.size >= block.size);
		block.next = next_block.self;
		block.next_valid = 1;
		next_block.prev = block.self;
		next_block.prev_valid = 1;
		next_block.put(table + next_block.self);
	}
	else{
		block.next_valid = 0;
		put_max_size(pdt,block.size);
	}
	if (prev_block.size){
		assert(prev_block.size <= block.size);
		block.prev = prev_block.self;
		block.prev_valid = 1;
		prev_block.next = block.self;
		prev_block.next_valid = 1;
		prev_block.put(table + prev_block.self);
	}
	else{
		block.prev_valid = 0;
		assert(next_block.size == 0 || pdt.head == next_block.self);
		pdt.head = block.self;
	}
	block.put(table + block.self);
}

#ifndef NDEBUG
void virtual_space::check_integrity(PDT& pdt,PT* table){
	assert(is_locked());
	bool free_map[0x200] = {0};
	unsigned free_count = 0;
	for (unsigned i = 0;i < 0x200;++i){
		if (!table[i].present && !table[i].preserve){
			assert(!table[i].bypass);
			free_map[i] = true;
			++free_count;
		}
	}
	auto max_size = get_max_size(pdt);
	if (!max_size){
		assert(free_count == 0);
		return;
	}
	BLOCK block;
	word last_size = 0;
	word last_block;
	block.get(table + pdt.head);
	assert(!block.prev_valid);
	do{
		assert(block.size);
		if (block.size == 1)
			assert(!block.prev_valid);
		if (block.prev_valid){
			assert(block.prev == last_block);
		}
		assert(last_size <= block.size);
		assert(free_count >= block.size);
		free_count -= block.size;
		for (unsigned i = 0;i < block.size;++i){
			assert(free_map[block.self + i]);
			free_map[block.self + i] = false;
		}
		if (block.self){
			auto& cur = table[block.self - 1];
			assert(cur.present || cur.preserve);
		}
		if (block.self + block.size < 0x200){
			auto& cur = table[block.self + block.size];
			assert(cur.present || cur.preserve);
		}
		last_size = block.size;
		last_block = block.self;
		if (!block.next_valid)
			break;
		block.get(table + block.next);
	}while(true);
	assert(0 == free_count);
	for (auto& ele : free_map){
		assert(!ele);
	}
	auto origin_remain = pdt.remain;
	put_max_size(pdt,last_size);
	assert(pdt.remain == origin_remain);
}
#endif

qword virtual_space::imp_reserve_any(PDT& pdt,PT* table,qword base_addr,word count){
	assert(is_locked());
	assert(count && table);
	assert(pdt.present && !pdt.bypass);
	if (get_max_size(pdt) < count)
		return 0;
	auto cur = pdt.head;
	BLOCK block;
	do{
		block.get(table + cur);
		assert(block.self == cur);
		if (block.size >= count){
			resolve_prior(pdt,table,block);
			erase(pdt,table,block);
			qword allocated_block = block.self + block.size - count;
			assert(allocated_block + count <= 0x200);
			for (unsigned i = 0;i < count;++i){
				assert(!table[allocated_block + i].present && !table[allocated_block + i].preserve);
				table[allocated_block + i].preserve = 1;
			}
			block.size -= count;
			insert(pdt,table,block);
#ifndef NDEBUG
	check_integrity(pdt,table);
#endif
			return base_addr + (allocated_block << 12);
		}
		cur = block.next;
	}while (block.next_valid);
	bugcheck("virtual_space corrupted %p",base_addr);
}

bool virtual_space::imp_reserve_fixed(PDT& pdt,PT* table,word index,word count){
	assert(is_locked());
	assert(count && table && index + count <= 0x200);
	assert(pdt.present && !pdt.bypass && get_max_size(pdt) > 0);

	if (table[index].present || table[index].preserve)
		return false;
	BLOCK blocks[2];
	blocks[0].get(table + index);
	assert(index >= blocks[0].self);
	if (index + count > blocks[0].self + blocks[0].size)
		return false;
	resolve_prior(pdt,table,blocks[0]);
	erase(pdt,table,blocks[0]);
	for (unsigned i = 0;i < count;++i){
		assert(!table[index + i].present && !table[index + i].preserve);
		table[index + i].preserve = 1;
	}
	blocks[1].size = blocks[0].size + blocks[0].self;
	blocks[1].self = index + count;
	blocks[1].size -= blocks[1].self;
	blocks[0].size = index - blocks[0].self;

	if (blocks[1].size > blocks[0].size){
		swap(blocks[1].self,blocks[0].self);
		swap(blocks[1].size,blocks[0].size);
	}
	blocks[1].prev_valid = 0;
	blocks[1].next_valid = 0;

	insert(pdt,table,blocks[0]);
	insert(pdt,table,blocks[1]);

#ifndef NDEBUG
	check_integrity(pdt,table);
#endif

	return true;
}

void virtual_space::imp_release(PDT& pdt,PT* table,qword base_addr,word count){
	assert(is_locked());
	assert(count && table && base_addr);
	word index = (base_addr >> 12) & 0x1FF;
	assert(index + count <= 0x200);
	assert(pdt.present && !pdt.bypass);

	BLOCK block;
	bool has_prior = (index && !table[index - 1].present && !table[index - 1].preserve);
	bool has_after = (index + count < 0x200 && !table[index + count].present && !table[index + count].preserve);
	if (has_prior){
		//prior block free
		if (has_after){
			//both prior & after are free
			//remove after block first
			block.get(table + index + count);
			resolve_prior(pdt,table,block);
			auto removed_size = block.size;
			erase(pdt,table,block);
			//then merge into prior block
			block.get(table + index - 1);
			resolve_prior(pdt,table,block);
			erase(pdt,table,block);
			block.size += removed_size + count;
		}
		else{
			//only prior block free
			block.get(table + index - 1);
			resolve_prior(pdt,table,block);
			erase(pdt,table,block);
			block.size += count;
		}
	}
	else{
		word new_size = count;
		block.prev_valid = 0;
		block.next_valid = 0;
		if (has_after){
			//only after block free
			//remove after block
			block.get(table + index + count);
			resolve_prior(pdt,table,block);
			erase(pdt,table,block);
			new_size += block.size;
		}
		//else standalone, use count as size

		block.self = index;
		block.size = new_size;
	}

	auto addr = base_addr;
	for (unsigned i = 0;i < count;++i){
		auto& cur = table[index + i];
		assert(!cur.bypass);
		if (cur.present && cur.page_addr){
			cur.present = 0;
			pm.release(cur.page_addr << 12);
			cur.page_addr = 0;
			invlpg((void*)addr);
			--used_pages;
		}
		else if (!cur.preserve){
			bugcheck("double release %p",addr);
		}
		cur.preserve = 0;
		addr += PAGE_SIZE;
	}
	insert(pdt,table,block);

#ifndef NDEBUG
	check_integrity(pdt,table);
#endif
}

dword virtual_space::imp_iterate(const PDPT* pdpt_table,qword base_addr,dword page_count,PTE_CALLBACK callback,qword data){
	assert(is_locked());
	assert(pdpt_table && page_count && callback);
	assert(base_addr && 0 == (base_addr & PAGE_MASK));
	map_view pdt_view;
    map_view pt_view;

	auto pdpt_index = (base_addr >> 30) & 0x1FF;
    auto pdt_index = (base_addr >> 21) & 0x1FF;
	auto off = (base_addr >> 12) & 0x1FF;
	dword count = 0;
	auto addr = base_addr;
	while(count < page_count){
		if (!pdpt_table[pdpt_index].present){
			return count;
		}
        pdt_view.map(pdpt_table[pdpt_index].pdt_addr << 12);
        PDT* pdt_table = (PDT*)pdt_view;
        while(count < page_count){
            auto& cur = pdt_table[pdt_index];
            if (cur.bypass || !cur.present){
                return count;
            }
            pt_view.map(cur.pt_addr << 12);
            PT* table = (PT*)pt_view;
            while(off < 0x200){
				if (!callback(table[off],addr,data))
					return count;
				addr += PAGE_SIZE;
				++off;
				if (++count == page_count)
					return count;
			}
			off = 0;
			if (++pdt_index == 0x200){
				pdt_index = 0;
				break;
			}
		}
		++pdpt_index;
		assert(pdpt_index < 0x200);
	}
	return count;
}


bool virtual_space::new_pdt(PDPT& pdpt,map_view& view){
	assert(!pdpt.present);
	auto phy_addr = pm.allocate();
	if (!phy_addr)
		return false;
	view.map(phy_addr);
	PDT* table = (PDT*)view;
	zeromemory(table,PAGE_SIZE);

	pdpt = {0};
	pdpt.pdt_addr = phy_addr >> 12;
	pdpt.user = 1;
	pdpt.write = 1;
	pdpt.present = 1;

	++used_pages;
	return true;
}

bool virtual_space::new_pt(PDT& pdt,map_view& view,bool take){
	assert(!pdt.present && !pdt.bypass);
	auto phy_addr = pm.allocate(take ? PM::TAKE : PM::NONE);
	if (!phy_addr)
		return false;
	view.map(phy_addr);
	PT* table = (PT*)view;
	zeromemory(table,PAGE_SIZE);
	pdt = {0};
	pdt.pt_addr = phy_addr >> 12;
	pdt.user = 1;
	pdt.write = 1;
	put_max_size(pdt,0x200);
	pdt.present = 1;

	BLOCK block = {0};
	block.size = 0x200;
	block.put(table);
	
	++used_pages;
	return true;
}

qword virtual_space::reserve_any(PDPT* pdpt_table,dword page_count){
	assert(is_locked());
	assert(page_count <= 0x200);
	map_view pdt_view;
	map_view pt_view;
	for (unsigned pdpt_index = 0;pdpt_index < 0x200;++pdpt_index){
		if (!pdpt_table[pdpt_index].present){   //allocate new PDT
			if (!new_pdt(pdpt_table[pdpt_index],pdt_view))
				break;
		}
		else{
			pdt_view.map(pdpt_table[pdpt_index].pdt_addr << 12);
		}
		PDT* pdt_table = (PDT*)pdt_view;
		for (unsigned i = 0;i < 0x200;++i){
			auto& cur = pdt_table[i];
			if (cur.bypass)
				continue;
			if (!cur.present){  //allocate new PT
				if (!new_pt(cur,pt_view,false))
					continue;
			}
			else{
				pt_view.map(cur.pt_addr << 12);
			}
			PT* table = (PT*)pt_view;
			qword base_addr = pdpt_index*0x40000000 + i*0x200000;
			auto res = imp_reserve_any(cur,table,base_addr,page_count);
			if (res)
				return res;
		}
	}
	return 0;
}

bool virtual_space::reserve_fixed(PDPT* pdpt_table,qword base_addr,dword page_count){
	assert(is_locked());
	assert(base_addr && page_count);
	assert(0 == (base_addr & PAGE_MASK));

	map_view pdt_view;
	map_view pt_view;

	auto pdpt_index = (base_addr >> 30) & 0x1FF;
	auto pdt_index = (base_addr >> 21) & 0x1FF;
	auto addr = base_addr;
	dword count = 0;
	while (count < page_count){
		if (!pdpt_table[pdpt_index].present){
			if (!new_pdt(pdpt_table[pdpt_index],pdt_view))
				goto rollback;
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
				if (!new_pt(cur,pt_view,false))
					goto rollback;
			}
			else{
				pt_view.map(cur.pt_addr << 12);
			}
			PT* table = (PT*)pt_view;
			dword off = (addr >> 12) & 0x1FF;
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
		safe_release(pdpt_table,base_addr,count);
	return false;
}

qword virtual_space::reserve_big(PDPT* pdpt_table,dword pagecount){
	assert(is_locked());
	auto aligned_count = align_up(pagecount,0x200) / 0x200;
	map_view pdt_view;
	for (unsigned pdpt_index = 0;pdpt_index < 0x200;++pdpt_index){
		if (!pdpt_table[pdpt_index].present){
			if (!new_pdt(pdpt_table[pdpt_index],pdt_view))
				break;
		}
		else{
			pdt_view.map(pdpt_table[pdpt_index].pdt_addr << 12);
		}
		PDT* pdt_table = (PDT*)pdt_view;
		unsigned avl_base = 0;
		dword avl_pages = 0;
		dword reserve_count = 0;
		for (unsigned i = 0;i < 0x200;++i){
			auto& cur = pdt_table[i];
			if (cur.bypass || (cur.present && get_max_size(cur) != 0x200)){
				avl_base = i + 1;
				avl_pages = 0;
				reserve_count = 0;
			}
			else{
				if (cur.present){
					assert(get_max_size(cur) == 0x200);
				}
				else{
					++reserve_count;
				}
				if (++avl_pages == aligned_count){
					break;
				}
			}
		}
		if (avl_pages == aligned_count){
			assert(avl_base + avl_pages <= 0x200);
			if (!pm.reserve(reserve_count))
				continue;
			qword base_addr = pdpt_index*0x40000000 + avl_base*0x200000;
			map_view pt_view;
			for (unsigned i = 0;i < avl_pages;++i){
				assert(pagecount);
				auto& cur = pdt_table[avl_base + i];
				assert(!cur.bypass);
				//TODO optimise
				if (cur.present){
					assert(get_max_size(cur) == 0x200);
					pt_view.map(cur.pt_addr << 12);
				}
				else{
					assert(reserve_count);
					new_pt(cur,pt_view,true);
					--reserve_count;
				}
				PT* table = (PT*)pt_view;
				auto res = imp_reserve_fixed(cur,table,0,min((dword)0x200,pagecount));
				if (!res)
					bugcheck("imp_reserve_fixed failed @ %p",cur);
				pagecount -= min((dword)0x200,pagecount);
			}
			assert(reserve_count == 0);
			return base_addr;
		}
	}
	return 0;
}

void virtual_space::safe_release(PDPT* pdpt_table,qword base_addr,dword page_count){
	assert(is_locked());
	assert(base_addr && page_count);
	assert(0 == (base_addr & PAGE_MASK));

	map_view pdt_view;
	map_view pt_view;

	auto pdpt_index = (base_addr >> 30) & 0x1FF;
	auto pdt_index = (base_addr >> 21) & 0x1FF;
	auto addr = base_addr;
	dword count = 0;
	while(count < page_count){
		if (!pdpt_table[pdpt_index].present){
			bugcheck("PDT not present %x",pdpt_table[pdpt_index]);
		}
		pdt_view.map(pdpt_table[pdpt_index].pdt_addr << 12);
		PDT* pdt_table = (PDT*)pdt_view;
		while(count < page_count){
			auto& cur = pdt_table[pdt_index];
			if (cur.bypass || !cur.present){
				bugcheck("cannot release page %x",cur);
			}
			pt_view.map(cur.pt_addr << 12);
			PT* table = (PT*)pt_view;
			dword off = (addr >> 12) & 0x1FF;
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