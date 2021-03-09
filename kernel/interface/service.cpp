#include "types.hpp"
#include "lang.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"
#include "sync/include/lock_guard.hpp"
using namespace UOS;

extern "C"
qword kernel_service(qword cmd,qword arg,qword* context){
	this_core core;
	auto this_thread = core.this_thread();
	auto this_process = this_thread->get_process();
	auto vspace = this_process->vspace;
	switch(cmd){
		case 0:
		{	//dbgprint
			string str;
			{
				interrupt_guard<virtual_space> guard(*vspace);
				auto pt = vspace->peek(arg);
				if (pt.present && pt.user){
					auto bound = (const char*)(align_down(arg,PAGE_SIZE) + PAGE_SIZE);
					auto ptr = (const char*)arg;
					while(ptr < bound && *ptr){
						str.push_back(*ptr++);
					}
				}
			}
			if (!str.empty()){
				dbgprint(str.c_str());
				return 0;
			}
			break;
		}
		case 0x0100:	//self
			return this_process->id;
		case 0x0108:	//sleep
			thread::sleep(arg);
			return 0;
		case 0x0118:	//exit
			thread::kill(core.this_thread());
			bugcheck("thread exit failed");
		case 0x0120:	//kill
		{
			process* ps;
			if (arg == 0)
				break;
			{
				interrupt_guard<handle_table> guard(this_process->handles);
				auto obj = (this_process->handles)[arg];
				if (!obj || obj->type() != waitable::PROCESS)
					break;
				ps = static_cast<process*>(obj);
			}
			ps->kill(0);
			return 0;
		}
		case 0x210:		//open_process
		{
			interrupt_guard<void> ig;
			this_process->acquire();
			if (arg == this_process->id){	//self
				return this_process->handles.put(this_process);
			}
			auto ps = proc.get(arg);
			if (ps == nullptr)
				break;
			auto handle = this_process->handles.put(ps);
			this_process->relax();
			return handle;
		}
		case 0x0220:	//try
			if (IS_HIGHADDR(arg))
				break;
			core.this_thread()->user_handler = arg;
			return 0;
	}
	return (-1);
}