#include "types.hpp"
#include "lang.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"
#include "sync/include/lock_guard.hpp"
using namespace UOS;

extern "C"
qword kernel_service(qword cmd,qword arg,qword* context){
	this_core core;
	auto vspace = core.this_thread()->get_process()->vspace;
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
			if (!str.empty())
				dbgprint(str.c_str());
			return str.empty() ? (-1) : 0;
		}
		case 0x0100:
		{	//sleep
			thread::sleep(arg);
			return 0;
		}
		case 0x0108:
		thread::exit();
	}
	return (-1);
}