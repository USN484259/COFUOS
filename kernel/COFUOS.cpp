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
#include "dev/include/rtc.hpp"
#include "dev/include/display.hpp"
#include "lang.hpp"

using namespace UOS;


//[[ noreturn ]]
//void AP_entry(word);


void print_sysinfo(void){
	dbgprint("%s",&sysinfo->sig);
	dbgprint("RSDT @ %p",0x00FFFFFFFFFFFFFF & sysinfo->ACPI_RSDT);
	dbgprint("PMM_avl_top @ %p",sysinfo->PMM_avl_top);
	dbgprint("kernel image %d pages",sysinfo->kernel_page);
	dbgprint("cpuid (eax == 7) -> 0x%x",(qword)sysinfo->cpuinfo);
	dbgprint("MAXPHYADDR %d",sysinfo->addrwidth & 0xFF);
	dbgprint("Video %d*%d*%d line_size = %d @ %p",\
		sysinfo->VBE_width,sysinfo->VBE_height,sysinfo->VBE_bpp,sysinfo->VBE_scanline,(qword)sysinfo->VBE_addr);
	dbgprint("FAT32 header @ %d, table @ %d, data @ %d, cluster %d",\
		sysinfo->FAT_header,sysinfo->FAT_table,sysinfo->FAT_data,sysinfo->FAT_cluster);
}



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

	constexpr qword table[] = {0x316,0x315};
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

void vm_test(void){
	__debugbreak();
	constexpr qword fixed_addr = HIGHADDR(0x3FDFE000);
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


[[ noreturn ]]
void krnlentry(void* module_base){
	buildIDT();
	kdb_init(sysinfo->ports[0]);

	print_sysinfo();

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
	//pm_test();
	//vm_test();

	_enable();

	constexpr auto w = 400;
	constexpr auto h = 300;
	auto buffer = new dword[w*h];
	assert(buffer);
	volatile qword tm = 0;
	rtc.set_handler([](qword tm,word ms,void* data){
		if (tm && ms == 0)
			*(qword*)data = tm;
	},(void*)&tm);

	while(true){
		__halt();
		if (tm){
			dbgprint("drawing...");
			srand(tm);
			Rect rect{-w/2,-h/2,w/2,h/2};
			for (unsigned i = 0;i < (w*h);++i){
				buffer[i] = rand();
			}
			auto res = display.draw(rect,buffer,4*w*h);
			assert(res);
			tm = 0;
		}
	}

	__debugbreak();
	BugCheck(not_implemented,0);
}
