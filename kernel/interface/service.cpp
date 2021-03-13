#include "service.hpp"
#include "lang.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"
#include "sync/include/lock_guard.hpp"
#include "dev/include/display.hpp"
#include "dev/include/rtc.hpp"

using namespace UOS;

#define OS_VERSION 0x00010000
#define OS_DESCRIPTION "COFUOS by USN484259"

inline void unpack_rect(rectangle& rect,qword lt,qword rb){
	rect = {(dword)lt,(dword)(lt >> 32),(dword)rb,(dword)(rb >> 32)};
}

inline qword pack_qword(dword a,dword b){
	return (qword)a | ((qword)b << 32);
}

memory_lock::memory_lock(virtual_space& vspace,void const* ptr,dword length,bool write) : guard(vspace){
	auto va = reinterpret_cast<qword>(ptr);
	auto tail = va + length;
	va = align_down(va,PAGE_SIZE);
	while(va < tail){
		auto pt = vspace.peek(va);
		if (!pt.present || !pt.user)
			return;
		if (write && !pt.write)
			return;
		va += PAGE_SIZE;
	}
	state = true;
}

handle_lock::handle_lock(handle_table& table,HANDLE handle,OBJTYPE type = UNKNOWN) : guard(table){
	if (handle == 0)
		return;
	auto ptr = table[handle];
	if (ptr == nullptr)
		return;
	if (type != UNKNOWN && ptr->type() != type)
		return;
	obj = ptr;
}

service_provider::service_provider(void){
	this_thread = core.this_thread();
	this_process = this_thread->get_process();
	vspace = this_process->vspace;
}

qword service_provider::os_info(void* buffer,dword limit){
	memory_lock lock(*vspace,buffer,limit,true);
	if (!lock.get())
		return BAD_BUFFER;
	auto size = sizeof(OS_INFO) + sizeof(OS_DESCRIPTION);
	if (limit < sizeof(OS_INFO))
		return pack_qword(TOO_SMALL,size);
	auto info = (OS_INFO*)buffer;
	info->version = OS_VERSION;
	info->features = 0;	//TODO
	info->active_core = cores.size();
	//TODO info->cpu_load
	info->process_count = proc.size();
	info->total_memory = PAGE_SIZE*pm.capacity();
	info->used_memory = info->total_memory - PAGE_SIZE*pm.available();
	info->reserved = 0;
	if (limit < size)
		return pack_qword(SUCCESS,sizeof(OS_INFO));
	memcpy(info + 1,OS_DESCRIPTION,sizeof(OS_DESCRIPTION));
	return pack_qword(SUCCESS,size);
}
qword service_provider::get_time(void){
	return rtc.get_time();
}
qword service_provider::enum_process(dword id){
	auto res = proc.enumerate(id);
	return pack_qword(res ? SUCCESS : FAILED,id);
}
qword service_provider::get_message(void* buffer,dword limit){
	//TODO
	bugcheck("not implemented");
}
STATUS service_provider::dbg_print(void const* buffer,dword length){
	memory_lock lock(*vspace,buffer,length);
	if (!lock.get())
		return BAD_BUFFER;
	dbgprint("%t",buffer,length);
	return SUCCESS;
}
STATUS service_provider::display_fill(dword color,qword lt,qword rb){
	if (this_process->get_privilege() < SHELL)
		return DENIED;
	rectangle rect;
	unpack_rect(rect,lt,rb);
	return display.fill(rect,color) ? SUCCESS : BAD_PARAM;
}
STATUS service_provider::display_draw(void const* buffer,qword lt,qword rb){
	if (this_process->get_privilege() < SHELL)
		return DENIED;
	rectangle rect;
	unpack_rect(rect,lt,rb);
	if (rect.right <= rect.left || rect.bottom <= rect.top)
		return BAD_PARAM;
	auto length = (rect.right - rect.left)*(rect.bottom - rect.top)*sizeof(dword);
	memory_lock lock(*vspace,buffer,length);
	if (!lock.get())
		return BAD_BUFFER;
	return display.draw(rect,(dword const*)buffer) ? SUCCESS : BAD_PARAM;
}
HANDLE service_provider::get_thread(void){
	interrupt_guard<void> ig;
	auto res = this_thread->acquire();
	assert(res);
	auto handle = this_process->handles.put(this_thread);
	if (!handle)
		this_thread->relax();
	return handle;
}
qword service_provider::thread_id(HANDLE handle){
	handle_lock lock(this_process->handles,handle,THREAD);
	auto th = static_cast<thread*>(lock.get());
	if (th == nullptr)
		return BAD_HANDLE;
	return pack_qword(SUCCESS,th->id);
}
qword service_provider::get_handler(void){
	return this_thread->user_handler;
}
qword service_provider::get_priority(HANDLE handle){
	handle_lock lock(this_process->handles,handle,THREAD);
	auto th = static_cast<thread*>(lock.get());
	if (th == nullptr)
		return BAD_HANDLE;
	return pack_qword(SUCCESS,th->get_priority());
}
void service_provider::exit_thread(void){
	thread::kill(this_thread);
	bugcheck("exit_thread failed");
}
STATUS service_provider::kill_thread(HANDLE handle){
	do{
		handle_lock lock(this_process->handles,handle,THREAD);
		auto th = static_cast<thread*>(lock.get());
		if (th == nullptr)
			return BAD_HANDLE;
		if (th == this_thread)
			break;
		thread::kill(th);
		return SUCCESS;
	}while(false);
	exit_thread();
}
STATUS service_provider::set_handler(qword handler){
	if (handler & HIGHADDR(0))
		return BAD_PARAM;
	this_thread->user_handler = handler;
	return SUCCESS;
}
STATUS service_provider::set_priority(HANDLE handle,byte val){
	handle_lock lock(this_process->handles,handle,THREAD);
	auto th = static_cast<thread*>(lock.get());
	if (th == nullptr)
		return BAD_HANDLE;
	if (val >= scheduler::idle_priority || val <= scheduler::kernel_priority)
		return BAD_PARAM;
	if (val <= scheduler::shell_priority && this_process->get_privilege() < SHELL)
		return DENIED;
	return th->set_priority(val) ? SUCCESS : BAD_PARAM;
}
qword service_provider::create_thread(qword entry,qword arg,dword stk_size){
	if (entry & HIGHADDR(0))
		return BAD_PARAM;
	if (stk_size == 0)
		stk_size = this_process->get_stack_preserve();
	auto count = max<dword>(1,align_up(stk_size,PAGE_SIZE)/PAGE_SIZE);
	auto stk_base = vspace->reserve(0,count + 1);
	if (stk_base == 0)
		return NO_RESOURCE;
	auto res = vspace->commit(stk_base + count*PAGE_SIZE,1);
	if (!res){
		vspace->release(stk_base,count + 1);
		return NO_RESOURCE;
	}
	auto stk_top = stk_base + (count + 1)*PAGE_SIZE;
	qword args[4] = {entry,arg,stk_top,stk_size};
	interrupt_guard<void> ig;
	auto th = this_process->spawn(user_entry,args);
	if (th == nullptr)
		return FAILED;
	auto handle = this_process->handles.put(th);
	if (handle)
		return pack_qword(SUCCESS,handle);
	th->relax();
	return NO_RESOURCE;
}
void service_provider::sleep(qword us){
	thread::sleep(us);
}
qword service_provider::check(HANDLE handle){
	handle_lock lock(this_process->handles,handle);
	auto obj = lock.get();
	if (obj == nullptr)
		return BAD_HANDLE;
	return pack_qword(SUCCESS,obj->check() ? 1 : 0);
}
qword service_provider::wait_for(HANDLE handle,qword us){
	if (handle == 0)
		return BAD_HANDLE;
	interrupt_guard<void> ig;
	this_process->handles.lock();
	auto obj = this_process->handles[handle];
	if (obj == nullptr){
		this_process->handles.unlock();
		return BAD_HANDLE;
	}
	return pack_qword(SUCCESS,obj->wait(us,&this_process->handles));
}
HANDLE service_provider::get_process(void){
	interrupt_guard<void> ig;
	auto res = this_process->acquire();
	assert(res);
	auto handle = this_process->handles.put(this_process);
	if (!handle)
		this_process->relax();
	return handle;
}
qword service_provider::process_id(HANDLE handle){
	handle_lock lock(this_process->handles,handle,PROCESS);
	auto ps = static_cast<process*>(lock.get());
	if (ps == nullptr)
		return BAD_HANDLE;
	return pack_qword(SUCCESS,ps->id);
}
qword service_provider::process_info(HANDLE handle,void* buffer,dword limit){
	handle_lock hlock(this_process->handles,handle,PROCESS);
	auto ps = static_cast<process*>(hlock.get());
	if (ps == nullptr)
		return BAD_HANDLE;
	memory_lock mlock(*vspace,buffer,limit,true);
	if (!mlock.get())
		return BAD_BUFFER;
	if (limit < sizeof(PROCESS_INFO))
		return pack_qword(TOO_SMALL,sizeof(PROCESS_INFO));
	auto info = (PROCESS_INFO*)buffer;
	info->id = ps->id;
	info->privilege = ps->get_privilege();
	info->state = ps->check() ? 0 : 1;
	info->thread_count = ps->size();
	info->handle_count = ps->handles.size();
	// TODO start_time & cpu_time_ms
	return pack_qword(SUCCESS,sizeof(PROCESS_INFO));
}
qword service_provider::get_command(HANDLE handle,void* buffer,dword limit){
	handle_lock hlock(this_process->handles,handle,PROCESS);
	auto ps = static_cast<process*>(hlock.get());
	if (ps == nullptr)
		return BAD_HANDLE;
	memory_lock mlock(*vspace,buffer,limit,true);
	if (!mlock.get())
		return BAD_BUFFER;
	auto size = ps->commandline.size() + 1;
	if (limit < size)
		return pack_qword(TOO_SMALL,size);
	memcpy(buffer,ps->commandline.c_str(),size);
	return pack_qword(SUCCESS,size);
}
void service_provider::exit_process(dword result){
	this_process->kill(result);
	bugcheck("exit_process failed");
}
STATUS service_provider::kill_process(HANDLE handle,dword result){
	do{
		handle_lock lock(this_process->handles,handle,PROCESS);
		auto ps = static_cast<process*>(lock.get());
		if (ps == nullptr)
			return BAD_HANDLE;
		if (ps == this_process)
			break;
		ps->kill(result);
		return SUCCESS;
	}while(false);
	exit_process(result);
}
qword service_provider::process_result(HANDLE handle){
	handle_lock lock(this_process->handles,handle,PROCESS);
	auto ps = static_cast<process*>(lock.get());
	if (ps == nullptr)
		return BAD_HANDLE;
	dword result;
	if (!ps->get_result(result))
		return FAILED;
	return pack_qword(SUCCESS,result);
}
qword service_provider::create_process(void const* info,dword length){
	//TODO
	bugcheck("not implemented");
}
qword service_provider::open_process(dword id){
	interrupt_guard<void> ig;
	auto ps = proc.get(id);
	if (ps == nullptr)
		return BAD_PARAM;
	if (this_process->get_privilege() < ps->get_privilege()){
		ps->relax();
		return DENIED;
	}
	auto handle = this_process->handles.put(ps);
	if (handle)
		return pack_qword(SUCCESS,handle);
	ps->relax();
	return NO_RESOURCE;
}
OBJTYPE service_provider::handle_type(HANDLE handle){
	handle_lock lock(this_process->handles,handle);
	auto obj = lock.get();
	if (obj == nullptr)
		return UNKNOWN;
	return obj->type();
}
STATUS service_provider::close_handle(HANDLE handle){
	if (handle == 0)
		return BAD_HANDLE;
	return this_process->handles.close(handle) ? SUCCESS : BAD_HANDLE;
}
qword service_provider::vm_peek(qword va){
	auto pt = vspace->peek(va);
	return *reinterpret_cast<qword const*>(&pt);
}
STATUS service_provider::vm_protect(qword va,dword count,qword attrib){
	return vspace->protect(va,count,attrib) ? SUCCESS : BAD_PARAM;
}
qword service_provider::vm_reserve(qword va,dword count){
	return vspace->reserve(va,count);
}
STATUS service_provider::vm_commit(qword va,dword count){
	auto res = vspace->commit(va,count);
	if (res)
		return SUCCESS;
	if (pm.capacity() - pm.available() <= pm.get_critical_limit())
		return NO_RESOURCE;
	return BAD_PARAM;
}
STATUS service_provider::vm_release(qword va,dword count){
	return vspace->release(va,count) ? SUCCESS : BAD_PARAM;
}
/*
extern "C"
qword kernel_service(qword cmd,qword a1,qword a2,qword a3){
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
				auto pt = vspace->peek(a1);
				if (pt.present && pt.user){
					auto bound = (const char*)(align_down(a1,PAGE_SIZE) + PAGE_SIZE);
					auto ptr = (const char*)a1;
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
			thread::sleep(a1);
			return 0;
		case 0x0118:	//exit
			thread::kill(core.this_thread());
			bugcheck("thread exit failed");
		case 0x0120:	//kill
		{
			process* ps;
			if (a1 == 0)
				break;
			{
				interrupt_guard<handle_table> guard(this_process->handles);
				auto obj = (this_process->handles)[a1];
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
			if (a1 == this_process->id){	//self
				return this_process->handles.put(this_process);
			}
			auto ps = proc.get(a1);
			if (ps == nullptr)
				break;
			auto handle = this_process->handles.put(ps);
			this_process->relax();
			return handle;
		}
		case 0x0220:	//try
			if (IS_HIGHADDR(a1))
				break;
			core.this_thread()->user_handler = a1;
			return 0;
	}
	return (-1);
}
*/