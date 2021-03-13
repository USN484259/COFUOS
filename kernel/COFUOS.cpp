#include "types.h"
#include "assert.hpp"
#include "constant.hpp"
#include "sysinfo.hpp"
#include "dev/include/cpu.hpp"
#include "pe64.hpp"
#include "process/include/core_state.hpp"
#include "process/include/thread.hpp"
#include "process/include/process.hpp"
#include "lang.hpp"
#include "sync/include/mutex.hpp"
#include "sync/include/lock_guard.hpp"

using namespace UOS;


//[[ noreturn ]]
//void AP_entry(word);

void thread_test(qword ptr,qword,qword,qword){
	auto m = reinterpret_cast<mutex*>(ptr);
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

void thread_spawner(qword ptr,qword,qword,qword){
	this_core core;
	thread* this_thread = core.this_thread();
	process* ps = this_thread->get_process();
	dbgprint("thread_spawner #%d",this_thread->id);
	qword args[4] = {ptr};
	for (auto i = 0;i < 0x10;++i){
		auto th = ps->spawn(thread_test,args);
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
	fpu_init();
	
	//kernel image properly formed, header should less than 0x400
	pe_kernel = PE64::construct(module_base,0x400);
	assert(pe_kernel->imgbase == (qword)module_base);

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

	auto th = proc.spawn("/test.exe p");
	auto pid = th->id;
	th->relax();
	proc.spawn("/test.exe")->relax();
	proc.spawn("/test.exe l")->relax();
	proc.spawn("/test.exe f")->relax();
	proc.spawn("/test.exe y")->relax();
	string str("/test.exe ");
	str.push_back('0' + pid);
	proc.spawn(move(str))->relax();
	proc.spawn("/test.exe v")->relax();

	if(false){
		this_core core;
		process* ps = core.this_thread()->get_process();
		mutex* m = new mutex();
		qword args[4] = {reinterpret_cast<qword>(m)};
		ps->spawn(thread_spawner,args);
	}

	//as idle thread
	while(true){
		halt();
	}
}