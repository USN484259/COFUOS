#include "types.hpp"
#include "assert.hpp"
#include "constant.hpp"
#include "sysinfo.hpp"
#include "hal.hpp"
#include "pe64.hpp"
#include "memory/include/vm.hpp"
#include "memory/include/pm.hpp"
#include "memory/include/heap.hpp"
#include "exception/include/kdb.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"
#include "dev/include/rtc.hpp"
#include "dev/include/display.hpp"
#include "dev/include/ps_2.hpp"
//#include "cui.hpp"
#include "lang.hpp"
#include "sync/include/mutex.hpp"
#include "sync/include/lock_guard.hpp"

using namespace UOS;


//[[ noreturn ]]
//void AP_entry(word);


void print_sysinfo(void){
	dbgprint("%s",&sysinfo->sig);
	dbgprint("%cSDT @ %p",sysinfo->rsdp.version ? 'X' : 'R', sysinfo->rsdp.address);
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
	int_trap(3);
	
	size_t page_count = 0;
	do{
		qword addr = pm.allocate();
		dbgprint("allocated %p, %d/%d",addr,pm.available(),pm.capacity());
		if (0 == addr)
			break;
		addr >>= 12;
		unitest_buffer[addr >> 3] |= (1 << (addr & 7));
		++page_count;
	}while(true);
	int_trap(3);

	constexpr qword table[] = {0x316,0x315};
	for (auto index : table){
		dbgprint("releasing page %x, %d remaining",index,--page_count);
		unitest_buffer[index >> 3] &= ~(1 << (index & 7));
		pm.release(index << 12);
	}
	int_trap(3);
	while(page_count){
		qword tsc = rdtsc();
		dbgprint("random from TSC : %x",tsc);
		tsc &= ((1 << 15) - 1);
		auto index = tsc;
		do{
			if (unitest_buffer[index >> 3] & (1 << (index & 7)))
				break;
			if (++index == (1 << 15))
				index = 0;
			if (index == tsc){
				bugcheck("page not found:%x",tsc);
			}
		}while(true);
		dbgprint("releasing page %x, %d remaining",index,--page_count);
		unitest_buffer[index >> 3] &= ~(1 << (index & 7));
		pm.release(index << 12);


	}
	int_trap(3);
}

void vm_test(void){
	int_trap(3);
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
	int_trap(3);
	for (qword i = 0;i < committed;++i){
		//probe every committed page
		auto ptr = (qword*)(fixed_addr + fixed_offset + PAGE_SIZE*i);
		dbgprint("Probing %p",ptr);
		*ptr = i;
	}
	int_trap(3);
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

	int_trap(3);
}

void thread_test(void* ptr){
	auto m = (mutex*)ptr;
	this_core core;
	auto this_thread = core.this_thread();
	
	while(true){
		{
			lock_guard<mutex> guard(*m);
			dbgprint("thread %d",this_thread->id);
			thread::sleep(rand()%(1000*1000));
		}
		thread::sleep(rand()%(1000*1000));
	}
	//bugcheck("bugcheck_test with cnt = %d",cnt);
}

void thread_spawner(void* ptr){
	this_core core;
	thread* this_thread = core.this_thread();
	process* ps = this_thread->get_process();
	dbgprint("thread_spawner #%d",this_thread->id);
	for (auto i = 0;i < 0x10;++i){
		auto th = ps->spawn(thread_test,ptr);
		dbgprint("spawned thread %d",th->id);
	}
	thread::kill(this_thread);
}

typedef void (*global_constructor)(void);

extern "C"
global_constructor __CTOR_LIST__;

extern "C"
[[ noreturn ]]
void krnlentry(void* module_base){
	build_IDT();
	debug_stub.emplace(sysinfo->ports[0]);
	assert(sysinfo->sig == 0x004F464E49535953ULL);	//'SYSINFO\0'
	print_sysinfo();
	fpu_init();
	
	//kernel image properly formed, header should less than 0x400
	pe_kernel = PE64::construct(module_base,0x400);
	assert(pe_kernel->imgbase == (qword)module_base);
	//since image has already loaded, it is safe to access section table
	for (unsigned i = 0;i < pe_kernel->section_count;++i){
		const auto section = pe_kernel->get_section(i);
		assert(section);
		char buf[9];
		memcpy(buf,section->name,8);
		buf[8] = 0;
		char attr[4] = {' ',' ',' ',0};
		if (section->attrib & 0x80000000)
			attr[1] = 'W';
		if (section->attrib & 0x40000000)
			attr[0] = 'R';
		if (section->attrib & 0x20000000)
			attr[2] = 'X';
		dbgprint("%s @ %p size = 0x%x %s",buf,pe_kernel->imgbase + section->offset,(qword)section->datasize,attr);
	}
	{
		global_constructor* head = &__CTOR_LIST__;
		assert(head);
		auto tail = head;
		do{
			++tail;
		}while(*tail);
		--tail;
		while(head != tail){
			(*tail--)();
		}
	}

	int_trap(3);
	sti();

	/*
	//TODO spawn startup thread
	this_core core;
	process* ps = core.this_thread()->get_process();
	mutex* m = new mutex();
	ps->spawn(thread_spawner,m);
	*/
	auto pid = proc.spawn("/test.exe p")->id;
	proc.spawn("/test.exe");
	proc.spawn("/test.exe l");
	proc.spawn("/test.exe f");
	proc.spawn("/test.exe y");
	string str("/test.exe ");
	str.push_back('0' + pid);
	proc.spawn(str);
	proc.spawn("/test.exe v");
	//as idle thread & core service
	while(true){
		halt();
	}
}