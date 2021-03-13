#pragma once
#include "types.h"
#include "interface.h"
#include "process/include/core_state.hpp"
#include "memory/include/vm.hpp"
#include "process/include/process.hpp"
#include "sync/include/lock_guard.hpp"

namespace UOS{
	class memory_lock{
		interrupt_guard<virtual_space> guard;
		bool state = false;
	public:
		memory_lock(virtual_space& vs,void const* va,dword length,bool write = false);
		inline bool get(void) const{
			return state;
		}
	};

	class handle_lock{
		interrupt_guard<handle_table> guard;
		waitable* obj = nullptr;
	public:
		handle_lock(handle_table& table,HANDLE handle,OBJTYPE type);
		inline waitable* get(void) const{
			return obj;
		}
	};

	struct service_provider{
		this_core core;
		thread* this_thread;
		process* this_process;
		virtual_space* vspace;
		service_provider(void);

		qword os_info(void* buffer,dword limit);
		qword get_time(void);
		qword enum_process(dword id);
		qword get_message(void* buffer,dword limit);
		STATUS dbg_print(void const* buffer,dword length);
		STATUS display_fill(dword color,qword lt,qword rb);
		STATUS display_draw(void const* buffer,qword lt,qword rb);
		HANDLE get_thread(void);
		qword thread_id(HANDLE handle);
		qword get_handler(void);
		qword get_priority(HANDLE handle);
		[[ noreturn ]]
		void exit_thread(void);
		STATUS kill_thread(HANDLE handle);
		STATUS set_handler(qword handler);
		STATUS set_priority(HANDLE handle,byte val);
		qword create_thread(qword entry,qword arg,dword stk_size);
		void sleep(qword us);
		qword check(HANDLE handle);
		qword wait_for(HANDLE handle,qword us);
		HANDLE get_process(void);
		qword process_id(HANDLE handle);
		qword process_info(HANDLE handle,void* buffer,dword limit);
		qword get_command(HANDLE handle,void* buffer,dword limit);
		[[ noreturn ]]
		void exit_process(dword result);
		STATUS kill_process(HANDLE handle,dword result);
		qword create_process(void const* info,dword length);
		qword process_result(HANDLE handle);
		qword open_process(dword id);
		OBJTYPE handle_type(HANDLE handle);
		STATUS close_handle(HANDLE handle);
		qword vm_peek(qword va);
		STATUS vm_protect(qword va,dword count,qword attrib);
		qword vm_reserve(qword va,dword count);
		STATUS vm_commit(qword va,dword count);
		STATUS vm_release(qword va,dword count);
	};
}