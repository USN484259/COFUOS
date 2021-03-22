#include "types.h"
#include "assert.hpp"
#include "constant.hpp"
#include "sysinfo.hpp"
#include "dev/include/cpu.hpp"
#include "dev/include/ps_2.hpp"
#include "dev/include/timer.hpp"
#include "pe64.hpp"
#include "process/include/core_state.hpp"
#include "process/include/thread.hpp"
#include "process/include/process.hpp"
#include "lang.hpp"
#include "sync/include/pipe.hpp"
#include "lock_guard.hpp"
#include "interface/include/object.hpp"
#include "string.hpp"

using namespace UOS;


void thread_kdb(qword ptr,qword,qword,qword){
	auto kdb_pipe = reinterpret_cast<pipe*>(ptr);
	byte buffer[0x400];
	unsigned offset = 0;
	while(true){
		kdb_pipe->wait();
		auto len = kdb_pipe->read(buffer + offset,sizeof(buffer) - offset);
		auto tail = offset;
		offset += len;
		do{
			for (;tail < offset;++tail){
				if (buffer[tail] == 0){
					dbgprint("%t",buffer,tail);
					break;
				}
			}
			if (tail == offset)
				break;
			++tail;
			for (unsigned i = 0;tail + i < offset;++i){
				buffer[i] = buffer[tail + i];
			}
			offset -= tail;
			tail = 0;
		}while(true);
	}
}

void thread_shell(qword,qword,qword,qword){
	this_core core;
	auto this_process = core.this_thread()->get_process();
	auto kdb_pipe = new pipe(PAGE_SIZE,pipe::atomic_write);
	kdb_pipe->manage(nullptr);
	qword args[4] = {reinterpret_cast<qword>(kdb_pipe)};
	HANDLE th = this_process->spawn(thread_kdb,args);
	assert(th);
	auto dev_pipe = new pipe(0x400,pipe::owner_write | pipe::atomic_write);
	dev_pipe->manage(nullptr);
	ps2_device.set_handler([](void const* sor,dword len,void* ud){
		auto p = reinterpret_cast<pipe*>(ud);
		p->write(sor,len);
	},reinterpret_cast<void*>(dev_pipe));
	bool bad = false;
	while(true){
		process::startup_info info = {SHELL,dev_pipe,nullptr,kdb_pipe};
		HANDLE handle_shell = proc.spawn("/shell.exe","",info);
		if (!handle_shell)
			bugcheck("shell failed to launch");
		process* shell = nullptr;
		auto launch_time = timer.running_time();
		{
			lock_guard<handle_table> guard(this_process->handles);
			shell = (process*)this_process->handles[handle_shell];
			assert(shell);
		}
		shell->wait();
		dbgprint("WARNING shell exited with %x",(qword)shell->result);
		this_process->handles.close(handle_shell);
		if (timer.running_time() - launch_time < 0x800000){	//about 8 seconds
			if (bad)
				break;
			bad = true;
		}
		else
			bad = false;
		thread::sleep(1000*1000);
	}
	bugcheck("shell not stable");
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

	{
		this_core core;
		auto this_process = core.this_thread()->get_process();
		qword args[4];
		HANDLE th = this_process->spawn(thread_shell,args);
		if (th == 0)
			bugcheck("failed to spawn shell thread");
		this_process->handles.close(th);
	}

/*
	HANDLE pid = proc.spawn("/test.exe p","");
	dword pid;
	{
		lock_guard<handle_table> guard(ps->handles);
		auto ptr = ps->handles[pid];
		assert(ptr && ptr->type() == PROCESS);
		pid = ((process*)ptr)->id;
		ptr->relax();
	}
	char const* commands[] = {\
		"/test.exe",
		"/test.exe l",
		"/test.exe f",
		"/test.exe y",
		"/test.exe v",
		"/test.exe i"
	};
	for (auto cmd : commands){
		pid = proc.spawn(cmd,"");
		lock_guard<handle_table> guard(ps->handles);
		auto ptr = ps->handles[pid];
		assert(ptr && ptr->type() == PROCESS);
		ptr->relax();
	}

	string str("/test.exe ");
	str.push_back('0' + pid);
	pid = proc.spawn(literal(str.begin(),str.end()),"");
	{
		lock_guard<handle_table> guard(ps->handles);
		auto ptr = ps->handles[pid];
		assert(ptr && ptr->type() == PROCESS);
		ptr->relax();
	}
*/


	//as idle thread
	while(true){
		halt();
	}
}