#include "service.hpp"
#include "lang.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"
#include "lock_guard.hpp"
#include "dev/include/display.hpp"
#include "dev/include/rtc.hpp"
#include "dev/include/timer.hpp"
#include "object.hpp"
#include "sync/include/semaphore.hpp"
#include "sync/include/event.hpp"
#include "sync/include/pipe.hpp"

using namespace UOS;

#define OS_VERSION 0x00010000
#define OS_DESCRIPTION "COFUOS by USN484259"

inline void unpack_rect(rectangle& rect,qword lt,qword rb){
	rect = {(dword)lt,(dword)(lt >> 32),(dword)rb,(dword)(rb >> 32)};
}

inline qword pack_qword(dword a,dword b){
	return (qword)a | ((qword)b << 32);
}

memory_lock::memory_lock(void const* ptr,dword length,bool write){
	this_core core;
	auto this_thread = core.this_thread();
	{
		interrupt_guard<thread> guard(*this_thread);
		vspace = this_thread->get_process()->vspace;
		vspace->lock();
		this_thread->hold(thread::HOLD_VSPACE);
	}
	auto va = reinterpret_cast<qword>(ptr);
	auto tail = va + length;
	va = align_down(va,PAGE_SIZE);
	while(va < tail){
		auto pt = vspace->peek(va);
		if (!pt.present || !pt.user)
			return;
		if (write && !pt.write)
			return;
		va += PAGE_SIZE;
	}
	state = true;
}

memory_lock::~memory_lock(void){
	assert(vspace);
	this_core core;
	auto this_thread = core.this_thread();
	assert(vspace == this_thread->get_process()->vspace);
	assert(vspace->is_locked());
	interrupt_guard<thread> guard(*this_thread);
	vspace->unlock();
	this_thread->drop(thread::HOLD_VSPACE);
}

handle_lock::handle_lock(HANDLE handle,OBJTYPE type = UNKNOWN){
	if (handle == 0)
		return;
	this_core core;
	auto this_thread = core.this_thread();
	{
		interrupt_guard<thread> guard(*this_thread);
		table = &this_thread->get_process()->handles;
		table->lock();
		this_thread->hold(thread::HOLD_HANDLE);
	}
	auto ptr = (*table)[handle];
	if (ptr == nullptr)
		return;
	if (type != UNKNOWN){
		auto objtype = ptr->type();
		if (objtype != type){
			if (type != STREAM || (objtype != PIPE && objtype != FILE))
				return;
		}
	}
	obj = ptr;
}

handle_lock::~handle_lock(void){
	if (table){
		this_core core;
		auto this_thread = core.this_thread();
		assert(table == &this_thread->get_process()->handles);
		assert(table->is_locked());
		interrupt_guard<thread> guard(*this_thread);
		table->unlock();
		this_thread->drop(thread::HOLD_HANDLE);
	}
}

service_provider::service_provider(void){
	this_thread = core.this_thread();
	this_process = this_thread->get_process();
	vspace = this_process->vspace;
}

qword service_provider::os_info(void* buffer,dword limit){
	memory_lock lock(buffer,limit,true);
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
	info->running_time = timer.running_time();
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
STATUS service_provider::display_fill(dword color,qword lt,qword rb){
	if (this_process->get_privilege() > SHELL)
		return DENIED;
	rectangle rect;
	unpack_rect(rect,lt,rb);
	return display.fill(rect,color) ? SUCCESS : BAD_PARAM;
}
STATUS service_provider::display_draw(void const* buffer,qword lt,qword rb){
	if (this_process->get_privilege() > SHELL)
		return DENIED;
	rectangle rect;
	unpack_rect(rect,lt,rb);
	if (rect.right <= rect.left || rect.bottom <= rect.top)
		return BAD_PARAM;
	auto length = (rect.right - rect.left)*(rect.bottom - rect.top)*sizeof(dword);
	memory_lock lock(buffer,length);
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
dword service_provider::thread_id(HANDLE handle){
	handle_lock lock(handle,THREAD);
	auto th = static_cast<thread*>(lock.get());
	if (th == nullptr)
		return 0;
	return th->id;
}
qword service_provider::get_handler(void){
	return this_thread->user_handler;
}
dword service_provider::get_priority(HANDLE handle){
	handle_lock lock(handle,THREAD);
	auto th = static_cast<thread*>(lock.get());
	if (th == nullptr)
		return BAD_HANDLE;
	return th->get_priority();
}
void service_provider::exit_thread(void){
	thread::kill(this_thread);
	bugcheck("exit_thread failed");
}
STATUS service_provider::kill_thread(HANDLE handle){
	do{
		handle_lock lock(handle,THREAD);
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
	handle_lock lock(handle,THREAD);
	auto th = static_cast<thread*>(lock.get());
	if (th == nullptr)
		return BAD_HANDLE;
	if (val >= scheduler::idle_priority || val <= scheduler::kernel_priority)
		return BAD_PARAM;
	if (val <= scheduler::shell_priority && this_process->get_privilege() > SHELL)
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

	auto handle = this_process->spawn(user_entry,args);
	if (handle)
		return pack_qword(SUCCESS,handle);
	return NO_RESOURCE;
}
void service_provider::sleep(qword us){
	thread::sleep(us);
}
dword service_provider::check(HANDLE handle){
	handle_lock lock(handle);
	auto obj = lock.get();
	if (obj == nullptr)
		return BAD_HANDLE;
	return obj->check() ? 1 : 0;
}
dword service_provider::wait_for(HANDLE handle,qword us){
	handle_lock lock(handle);
	auto obj = lock.get();
	if (obj == nullptr)
		return BAD_HANDLE;
	lock.drop();
	return obj->wait(us,[](void){
		this_core core;
		auto this_thread = core.this_thread();
		auto& table = this_thread->get_process()->handles;
		assert(table.is_locked() && !table.is_exclusive());
		interrupt_guard<thread> guard(*this_thread);
		table.unlock();
		this_thread->drop(thread::HOLD_HANDLE);
	});
}
dword service_provider::signal(HANDLE handle,dword mode){
	handle_lock lock(handle);
	auto obj = lock.get();
	if (obj == nullptr)
		return BAD_HANDLE;
	switch(obj->type()){
		case SEMAPHORE:
			return static_cast<semaphore*>(obj)->signal();
		case EVENT:
		{
			auto ev = static_cast<event*>(obj);
			switch(mode){
				case 0:		//reset
					ev->reset();
					return 0;
				case 1:		//signal one
					return ev->signal_one();
				default:
					return ev->signal_all();
			}
		}
		default:
			return BAD_HANDLE;
	}
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
dword service_provider::process_id(HANDLE handle){
	handle_lock lock(handle,PROCESS);
	auto ps = static_cast<process*>(lock.get());
	if (ps == nullptr)
		return 0;
	return ps->id;
}
qword service_provider::process_info(HANDLE handle,void* buffer,dword limit){
	handle_lock hlock(handle,PROCESS);
	auto ps = static_cast<process*>(hlock.get());
	if (ps == nullptr)
		return BAD_HANDLE;
	memory_lock mlock(buffer,limit,true);
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
	info->start_time = ps->start_time;
	info->cpu_time = ps->cpu_time;
	info->memory_usage = ps->vspace->usage()*PAGE_SIZE;
	return pack_qword(SUCCESS,sizeof(PROCESS_INFO));
}
qword service_provider::get_command(HANDLE handle,void* buffer,dword limit){
	handle_lock hlock(handle,PROCESS);
	auto ps = static_cast<process*>(hlock.get());
	if (ps == nullptr)
		return BAD_HANDLE;
	memory_lock mlock(buffer,limit,true);
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
		handle_lock lock(handle,PROCESS);
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
	handle_lock lock(handle,PROCESS);
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
	if (this_process->get_privilege() > ps->get_privilege()){
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
	handle_lock lock(handle);
	auto obj = lock.get();
	if (obj == nullptr)
		return UNKNOWN;
	return obj->type();
}
qword service_provider::open_handle(const void* buffer,dword length){
	
	this_core core;
	auto this_process = core.this_thread()->get_process();

	memory_lock lock(buffer,length);
	if (!lock.get())
		return BAD_BUFFER;
	span<char> name((char const*)buffer,length);

	waitable* ptr = nullptr;
	{
		interrupt_guard<object_manager> guard(named_obj);
		auto obj = named_obj.get(name);
		if (obj == nullptr)
			return NOT_FOUND;
		if (this_process->get_privilege() > (byte)obj->property)
			return DENIED;
		ptr = obj->obj;
		if (!ptr->acquire())
			return NOT_FOUND;
		auto handle = this_process->handles.put(ptr);
		if (handle)
			return pack_qword(SUCCESS,handle);
	}
	ptr->relax();
	return NO_RESOURCE;
}
STATUS service_provider::close_handle(HANDLE handle){
	if (handle == 0)
		return BAD_HANDLE;
	return this_process->handles.close(handle) ? SUCCESS : BAD_HANDLE;
}
qword service_provider::create_object(OBJTYPE type,qword a1,qword a2){
	waitable* ptr = nullptr;
	interrupt_guard<void> ig;
	switch(type){
		case SEMAPHORE:
			if (a1)
				ptr = new semaphore(a1);
			break;
		case EVENT:
			ptr = new event(a1);
			break;
		case PIPE:
			if (a1 >= 0x10)
				ptr = new pipe(a1,a2);
			break;
		default:
			break;
	}
	if (!ptr)
		return BAD_PARAM;
	ptr->manage();
	auto handle = this_process->handles.put(ptr);
	if (handle)
		return pack_qword(SUCCESS,handle);
	ptr->relax();
	return NO_RESOURCE;
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
dword service_provider::iostate(HANDLE handle){
	handle_lock lock(handle,STREAM);
	auto obj = lock.get();
	if (obj == nullptr)
		return BAD_HANDLE;
	return static_cast<stream*>(obj)->state();
}
qword service_provider::read(HANDLE handle,void* buffer,dword limit){
	handle_lock hlock(handle,STREAM);
	auto obj = hlock.get();
	if (obj == nullptr)
		return BAD_HANDLE;
	auto f = static_cast<stream*>(obj);
	memory_lock mlock(buffer,limit,true);
	if (!mlock.get())
		return BAD_BUFFER;
	auto len = f->read(buffer,limit);
	return pack_qword(f->state(),len);
}
qword service_provider::write(HANDLE handle,void const* buffer,dword length){
	handle_lock hlock(handle,STREAM);
	auto obj = hlock.get();
	if (obj == nullptr)
		return BAD_HANDLE;
	auto f = static_cast<stream*>(obj);
	memory_lock mlock(buffer,length);
	if (!mlock.get())
		return BAD_BUFFER;
	auto len = f->write(buffer,length);
	return pack_qword(f->state(),len);
}