#include "types.hpp"
#include "assert.hpp"
#include "cpu/include/hal.hpp"
#include "constant.hpp"
#include "sysinfo.hpp"
#include "image/include/pe.hpp"
#include "memory/include/vm.hpp"
#include "memory/include/pm.hpp"
#include "memory/include/heap.hpp"
#include "exception/include/kdb.hpp"
#include "lang.hpp"

using namespace UOS;


//[[ noreturn ]]
//void AP_entry(word);



#ifdef PM_TEST
void pm_test(void){
	static byte unitest_buffer[PAGE_SIZE];
	zeromemory(unitest_buffer,PAGE_SIZE);
	__debugbreak();
	
	size_t page_count = 0;
	do{
		qword addr = pm.allocate(0x0F);
		dbgprint("allocated %p, %d/%d",addr,pm.available(),pm.capacity());
		if (0 == addr)
			break;
		addr >>= 12;
		unitest_buffer[addr >> 3] |= (1 << (addr & 7));
		++page_count;
	}while(true);
	__debugbreak();

	static const qword table[] = {0x316,0x315};
	for (auto index : table){
		dbgprint("releasing page %x, %d remaining",index,--page_count);
		unitest_buffer[index >> 3] &= ~(1 << (index & 7));
		pm.release(index << 12);
	}
	__debugbreak();
	while(page_count){
		qword tsc = __rdtsc();
		dbgprint("random from TSC : %x",tsc);
		tsc &= ((1 << 15) - 1);
		auto index = tsc;
		do{
			if (unitest_buffer[index >> 3] & (1 << (index & 7)))
				break;
			if (++index == (1 << 15))
				index = 0;
			if (index == tsc){
				dbgprint("page not found");
				BugCheck(corrupted,tsc);
			}
		}while(true);
		dbgprint("releasing page %x, %d remaining",index,--page_count);
		unitest_buffer[index >> 3] &= ~(1 << (index & 7));
		pm.release(index << 12);


	}
	__debugbreak();
}
#endif

#ifdef VM_TEST
void vm_test(void){
	__debugbreak();
	static const qword fixed_addr = HIGHADDR(0x3FDFE000);
	size_t fixed_size = 4*pm.capacity();
	qword fixed_offset = PAGE_SIZE*(fixed_size/0x10);
	dbgprint("Reserving 0x%x pages @ %p",fixed_size,fixed_addr);
	qword addr = vm.reserve(fixed_addr,fixed_size);
	assert(addr == fixed_addr);

	addr = vm.reserve(fixed_addr - fixed_size/2, fixed_size);	//reserve overlap area
	assert(0 == addr);

	addr = vm.reserve(HIGHADDR(0xB000),1);	//PT bypass
	assert(0 == addr);

	addr = vm.reserve(pe_kernel->imgbase,0x10);		//PDT bypass
	assert(0 == addr);

	addr = vm.reserve(HIGHADDR(0x007FFFFF0000),0x20);	//over 512G
	assert(0 == addr);

	addr = vm.reserve(0x10000000,0x10);	//LOWADDR
	assert(0 == addr);

	addr = vm.reserve(0,2*0x200*0x200);	//too big
	assert(0 == addr);

	qword big_addr = vm.reserve(0,0x2020);	//valid reserve_big
	assert(big_addr);

	addr = vm.reserve(0,0x200);	//valid reserve_any
	assert(addr);
	dbgprint("Reserved 0x200 pages @ %p",addr);

	bool res;

	res = vm.commit(fixed_addr,fixed_size);	//over commit
	assert(!res);
	
	res = vm.commit(fixed_addr - fixed_offset,0x10);	//commit to free page
	assert(!res);

	res = vm.commit(pe_kernel->imgbase + PAGE_SIZE*0x100,0x10);	//PDT bypass
	assert(!res);

	res = vm.commit(HIGHADDR(0xB000),1);	//PT bypass
	assert(!res);

	auto committed = pm.available() - 1;
	dbgprint("Committing 0x%x pages @ %p",committed,fixed_addr + fixed_offset);
	res = vm.commit(fixed_addr + fixed_offset,committed);	//normal commit
	assert(res);
	__debugbreak();
	for (qword i = 0;i < committed;++i){
		//probe every committed page
		auto ptr = (qword*)(fixed_addr + fixed_offset + PAGE_SIZE*i);
		dbgprint("Probing %p",ptr);
		*ptr = i;
	}
	__debugbreak();
	res = vm.protect(fixed_addr + fixed_offset,committed/2,PAGE_USER);	//invalid attrib
	assert(!res);

	res = vm.protect(fixed_addr,0x10,PAGE_WRITE);	//change attrib on non-commit page
	assert(!res);

	res = vm.protect(fixed_addr + fixed_offset,committed/2,0);	//valid attrib
	assert(res);

	for (qword i = 0;i < committed;++i){
		//probe every committed page
		auto ptr = (qword*)(fixed_addr + fixed_offset + PAGE_SIZE*i);
		dbgprint("Checking %p",ptr);
		assert(*ptr == i);
	}

	res = vm.release(HIGHADDR(0xB000),1);	//PT bypass
	assert(!res);

	res = vm.release(pe_kernel->imgbase,0x10);	//PDT bypass
	assert(!res);

	res = vm.release(fixed_addr - fixed_offset,0x10);	//release free page
	assert(!res);

	res = vm.release(fixed_addr + fixed_offset/2,fixed_offset);	//comb of reserved & free pages
	assert(!res);

	res = vm.release(fixed_addr,fixed_size);		//comb of reserved & committed pages
	assert(res);

	res = vm.release(addr,0x10);	//release in multiple steps
	assert(res);
	res = vm.release(big_addr,0x2020);	//release big pages
	assert(res);
	res = vm.release(addr + PAGE_SIZE*0x10,0x1F0);
	assert(res);

	__debugbreak();
}
#endif

[[ noreturn ]]
void krnlentry(void* module_base){
	buildIDT();
	kdb_init(sysinfo->ports[0]);

	pe_kernel = PE64::construct(module_base);
	assert(pe_kernel->imgbase == (qword)module_base);
	{
		procedure* global_constructor = nullptr;
		for (unsigned i = 0;i < pe_kernel->section_count;++i){
			const auto section = pe_kernel->get_section(i);
			assert(section);
			const char strCRT[8]={'.','C','R','T',0};
			if (8 == match(strCRT,section->name,8)){
				global_constructor = (procedure*)(pe_kernel->imgbase + section->offset);
				break;
			}
		}
		assert(global_constructor);
		while(*global_constructor){
			(*global_constructor++)();
		}
	}

#ifdef PM_TEST
	pm_test();
#endif

#ifdef VM_TEST
	vm_test();
#endif
	__debugbreak();
	BugCheck(not_implemented,0);
}
