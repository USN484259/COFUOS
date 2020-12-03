#include "vm.hpp"
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

using namespace UOS;

void invlpg(volatile void* p){
	__invlpg((void*)p);
}

void* VM::map_view(qword pa,qword attrib){
	assert(0 == (pa & PAGE_MASK));
	assert(0 == (attrib & 0x7FFFFFFFFFFFF004));	//not user, attributes only
	auto table = (volatile qword*)MAP_TABLE_BASE;
	for (unsigned index = 0; index < 0x200; ++index)
	{
		qword origin_value = table[index];
		if (origin_value & 0x01)
			continue;
		
		qword new_value = attrib | pa | PAGE_PRESENT;

		if (origin_value == cmpxchg(table + index, new_value, origin_value)){
			//gain this slot, returns VA
			return (void*)(MAP_VIEW_BASE + PAGE_SIZE * index);
		}
	}
	BugCheck(bad_alloc,table);
}

void VM::unmap_view(void* view){
	qword addr = (qword)view;
	assert(0 == (addr & PAGE_MASK));
	if (addr < MAP_VIEW_BASE)
		BugCheck(out_of_range,view);
	auto index = (addr - MAP_VIEW_BASE) >> 12;
	if (index >= 0x200)
		BugCheck(out_of_range,view);

	auto table = (volatile qword *)MAP_TABLE_BASE;
	qword origin_value = table[index];
	if (0 == origin_value & 0x01)
		BugCheck(corrupted, origin_value);
	if (origin_value != cmpxchg(table + index, (qword)0, origin_value))
		BugCheck(corrupted, origin_value);
	invlpg(view);
}

VM::kernel_vspace::kernel_vspace(void){
	//TODO
}

bool VM::kernel_vspace::peek(void* dest,qword addr,size_t count){
	//TODO
	return false;
}