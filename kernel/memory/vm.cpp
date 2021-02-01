#include "vm.hpp"
#include "pm.hpp"
#include "constant.hpp"
#include "util.hpp"
#include "assert.hpp"
#include "intrinsics.hpp"

using namespace UOS;

VM::map_view::map_view(void) : view(nullptr){}
VM::map_view::map_view(qword pa,qword attrib) : view(nullptr){
	map(pa,attrib);
}
VM::map_view::~map_view(void){
	unmap();
}

void VM::map_view::map(qword pa,qword attrib){
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
	BugCheck(bad_alloc,table);
}

void VM::map_view::unmap(void){
	if (!view)
		return;
	qword addr = (qword)view;
	assert(0 == (addr & PAGE_MASK));
	if (addr < MAP_VIEW_BASE)
		BugCheck(out_of_range,view);
	auto index = (addr - MAP_VIEW_BASE) >> 12;
	if (index >= 0x200)
		BugCheck(out_of_range,view);

	auto table = (qword volatile* const)MAP_TABLE_BASE;
	qword origin_value = table[index];
	if (0 == (origin_value & 0x01))
		BugCheck(corrupted, origin_value);
	if (origin_value != cmpxchg(table + index, (qword)0, origin_value))
		BugCheck(corrupted, origin_value);
	invlpg(view);
}

void VM::virtual_space::BLOCK::get(const VM::PT* pt){
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

void VM::virtual_space::BLOCK::put(PT* pt) const{
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


word VM::virtual_space::get_max_size(const PDT& pdt){
	word remain = pdt.remain;
	if (remain < 8)
		return remain ? (word)1 << (remain - 1) : (word)0;
	else
		return (word)(64 + 56*(remain - 7));
}

void VM::virtual_space::put_max_size(PDT& pdt,word max_size){
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

void VM::virtual_space::shift_left(PDT& pdt,PT* table,BLOCK& block){
	auto max_size = get_max_size(pdt);
	if (!block.next_valid)
		max_size = block.size;
	if (block.size){
		while(block.prev_valid){
			BLOCK prev_block;
			prev_block.get(table + block.prev);
			assert(prev_block.next_valid && prev_block.next == block.self);
			if (prev_block.size <= block.size)
				break;
			max_size = max(max_size,prev_block.size);

			prev_block.next = block.next;
			prev_block.next_valid = block.next_valid;

			block.prev = prev_block.prev;
			block.prev_valid = prev_block.prev_valid;

			block.next = prev_block.self;
			block.next_valid = 1;

			prev_block.prev = block.self;
			prev_block.prev_valid = 1;

			prev_block.put(table + prev_block.self);
		}
		block.put(table + block.self);
	}else {
		if (block.next_valid){
			BLOCK next_block;
			next_block.get(table + block.next);
			assert(next_block.prev_valid && next_block.prev == block.self);
			//max_size unchanged
			next_block.prev = block.prev;
			next_block.prev_valid = block.prev_valid;
			next_block.put(table + next_block.self);
		}
		if (block.prev_valid){
			BLOCK prev_block;
			prev_block.get(table + block.prev);
			assert(prev_block.next_valid && prev_block.next == block.self);
			max_size = prev_block.size;
			prev_block.next = block.next;
			prev_block.next_valid = block.next_valid;
			prev_block.put(table + prev_block.self);
		}
	}
	if (!block.prev_valid)
		pdt.head = block.self;
	put_max_size(pdt,max_size);
}

void VM::virtual_space::shift_right(PDT& pdt,PT* table,BLOCK& block){
	assert(block.size > 1);
	auto max_size = max(block.size,get_max_size(pdt));
	if (block.prev_valid){
		BLOCK prev_block;
		prev_block.get(table + block.prev);
		assert(prev_block.next_valid);
		if (prev_block.next != block.self){
			prev_block.next = block.self;
			prev_block.put(table + prev_block.self);
		}
	}
	while(block.next_valid){
		BLOCK next_block;
		next_block.get(table + block.next);
		assert(next_block.prev_valid);
		max_size = max(max_size,next_block.size);

		if (next_block.size >= block.size){
			if (next_block.prev != block.self){
				next_block.prev = block.self;
				next_block.put(table + next_block.self);
			}
			break;
		}
		next_block.prev = block.prev;
		next_block.prev_valid = block.prev_valid;

		block.next = next_block.next;
		block.next_valid = next_block.next_valid;

		block.prev = next_block.self;
		block.prev_valid = 1;

		next_block.next = block.self;
		next_block.next_valid = 1;

		next_block.put(table + next_block.self);
	}
	block.put(table + block.self);
	if (!block.prev_valid)
		pdt.head = block.self;
	put_max_size(pdt,max_size);
}

void VM::virtual_space::insert(PDT& pdt,PT* table,BLOCK& block,word hint){
	if (0 == block.size)
		return;
	auto max_size = get_max_size(pdt);
	if (max_size == 0){	//link empty, add as first element
		block.prev_valid = 0;
		block.next_valid = 0;
		block.put(table + block.self);
		pdt.head = block.self;
		max_size = block.size;
		put_max_size(pdt,max_size);
		return;
	}
	BLOCK prev_block;
	BLOCK next_block;
	prev_block.size = 0;
	prev_block.next = hint;
	do{
		next_block.get(table + prev_block.next);
		assert(prev_block.size || next_block.size >= block.size);
		if (next_block.size > block.size)
			break;
		if (prev_block.size && next_block.size == block.size)
			break;
		prev_block = next_block;
	}while(prev_block.next_valid);
	assert(prev_block.size);
	if (prev_block.next_valid){	//next_block valid
		assert(next_block.prev == prev_block.self);
		next_block.prev = block.self;
		next_block.prev_valid = 1;
		block.next = next_block.self;
		block.next_valid = 1;
		next_block.put(table + next_block.self);
	}
	else{	//prev block as end of link
		assert (prev_block.self == next_block.self);
		assert(block.size >= max_size);
		block.next_valid = 0;
		put_max_size(pdt,block.size);
	}
	block.prev = prev_block.self;
	block.prev_valid = 1;
	prev_block.next = block.self;
	prev_block.next_valid = 1;

	prev_block.put(table + prev_block.self);
	block.put(table + block.self);
}

#ifndef NDEBUG
void VM::virtual_space::check_integrity(PDT& pdt,PT* table){
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

qword VM::virtual_space::imp_reserve_any(VM::PDT& pdt,PT* table,qword base_addr,word count){
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
			qword allocated_block = block.self + block.size - count;
			assert(allocated_block + count <= 0x200);
			for (unsigned i = 0;i < count;++i){
				assert(!table[allocated_block + i].present && !table[allocated_block + i].preserve);
				table[allocated_block + i].preserve = 1;
			}
			block.size -= count;
			shift_left(pdt,table,block);
#ifndef NDEBUG
	check_integrity(pdt,table);
#endif
			return base_addr + (allocated_block << 12);
		}
		cur = block.next;
	}while (block.next_valid);
	BugCheck(corrupted,base_addr);
}

bool VM::virtual_space::imp_reserve_fixed(PDT& pdt,PT* table,word index,word count){
	assert(count && table && index + count <= 0x200);
	assert(pdt.present && !pdt.bypass && get_max_size(pdt) > 0);

	if (table[index].present || table[index].preserve)
		return false;
	BLOCK blocks[2];
	blocks[0].get(table + index);
	assert(index >= blocks[0].self);
	if (index + count > blocks[0].self + blocks[0].size)
		return false;

	for (unsigned i = 0;i < count;++i){
		assert(!table[index + i].present && !table[index + i].preserve);
		table[index + i].preserve = 1;
	}
	blocks[1].size = blocks[0].size + blocks[0].self;
	blocks[1].self = index + count;
	blocks[1].size -= blocks[1].self;
	blocks[0].size = index - blocks[0].self;

	if (!blocks[0].size || (blocks[1].size && blocks[0].size > blocks[1].size)){
		swap(blocks[0].self,blocks[1].self);
		swap(blocks[0].size,blocks[1].size);
	}

	shift_left(pdt,table,blocks[0]);
	insert(pdt,table,blocks[1],blocks[0].self);

#ifndef NDEBUG
	check_integrity(pdt,table);
#endif

	return true;
}

void VM::virtual_space::imp_release(PDT& pdt,PT* table,qword base_addr,word count){
	assert(count && table && base_addr);
	word index = (base_addr >> 12) & 0x1FF;
	assert(index + count <= 0x200);
	assert(pdt.present && !pdt.bypass);
	auto addr = base_addr;
	for (unsigned i = 0;i < count;++i){
		auto& cur = table[index + i];
		assert(!cur.bypass);
		if (cur.present && cur.page_addr){
			cur.present = 0;
			pm.release(cur.page_addr << 12);
			cur.page_addr = 0;
			invlpg((void*)addr);
		}
		else if (!cur.preserve){
			BugCheck(corrupted,i);
		}
		cur.preserve = 0;
		addr += PAGE_SIZE;
	}
	BLOCK block;
	block.size = 0;
	//block.next_valid = block.prev_valid = 0;
	if (index && !table[index - 1].present && !table[index - 1].preserve){
		block.get(table + index - 1);
		block.size += count;
	}
	if (index + count < 0x200 && !table[index + count].present && !table[index + count].preserve){
		if (block.size){	//prev && next, remove next
			BLOCK remove_block;
			remove_block.get(table + index + count);
			//remove smaller block
			if (remove_block.size > block.size){
				swap(remove_block.next,block.next);
				swap(remove_block.next_valid,block.next_valid);
				swap(remove_block.prev,block.prev);
				swap(remove_block.prev_valid,block.prev_valid);
			}
			block.size += remove_block.size;
			remove_block.size = 0;
			//remove the block
			shift_left(pdt,table,remove_block);
		}
		else{	//next only
			block.get(table + index + count);
			block.self = index;
			block.size += count;
		}
	}
	if (block.size){	//merge into existing block
		shift_right(pdt,table,block);
	}
	else{	//insert new block
		block.self = index;
		block.size = count;
		insert(pdt,table,block,pdt.head);
	}
#ifndef NDEBUG
	check_integrity(pdt,table);
#endif
}

size_t VM::virtual_space::imp_iterate(const PDPT* pdpt_table,qword base_addr,size_t page_count,PTE_CALLBACK callback,qword data){
	assert(pdpt_table && page_count && callback);
	assert(base_addr && 0 == (base_addr & PAGE_MASK));
	map_view pdt_view;
    map_view pt_view;

	auto pdpt_index = (base_addr >> 30) & 0x1FF;
    auto pdt_index = (base_addr >> 21) & 0x1FF;
	auto off = (base_addr >> 12) & 0x1FF;
	size_t count = 0;
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