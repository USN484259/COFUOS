#include "service.hpp"
#include "lang.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"
#include "lock_guard.hpp"
#include "dev/include/acpi.hpp"
#include "dev/include/display.hpp"
#include "dev/include/rtc.hpp"
#include "dev/include/timer.hpp"
#include "dev/include/disk_interface.hpp"
#include "object.hpp"
#include "sync/include/semaphore.hpp"
#include "sync/include/event.hpp"
#include "sync/include/pipe.hpp"

using namespace UOS;

inline void unpack_rect(rectangle& rect,qword val){
	dword rb = (dword)(val >> 32);
	dword lt = (dword)val;

	rect = {(word)lt,(word)(lt >> 16),(word)rb,(word)(rb >> 16)};
}

inline qword pack_qword(dword a,dword b){
	return (qword)a | ((qword)b << 32);
}

service_provider::service_provider(void){
	this_thread = core.this_thread();
	this_process = this_thread->get_process();
	vspace = this_process->vspace;
	this_thread->hold();
}

service_provider::~service_provider(void){
	if (hold_memory)
		vspace->unlock();
	if (hold_handle)
		this_process->handles.unlock();
	if (!skip_critical)
		this_thread->drop();
}

bool service_provider::check(void const* ptr,dword length,bool write){
	if (!hold_memory){
		vspace->lock();
		hold_memory = true;
	}
	auto va = reinterpret_cast<qword>(ptr);
	auto tail = va + length;
	va = align_down(va,PAGE_SIZE);
	while(va < tail){
		auto pt = vspace->peek(va);
		if (!pt.present || !pt.user){
			return false;
		}
		if (write && !pt.write){
			return false;
		}
		va += PAGE_SIZE;
	}
	assert(0 == (tail & HIGHADDR(0)));
	return true;
}

waitable* service_provider::get(HANDLE handle,OBJTYPE type){
	if (!hold_handle){
		this_process->handles.lock();
		hold_handle = true;
	}
	if (handle == 0)
		return nullptr;
	auto ptr = (this_process->handles)[handle];
	if (ptr == nullptr)
		return nullptr;
	if (type != OBJ_UNKNOWN){
		auto objtype = ptr->type();
		if (objtype != type){
			if (type != OBJ_STREAM || (objtype != OBJ_PIPE && objtype != OBJ_FILE))
				return nullptr;
		}
	}
	return ptr;
}

qword service_provider::osctl(osctl_code cmd,void* buffer,dword length){
	if (this_process->get_privilege() > SHELL)
		return DENIED;
	bool write;
	switch(cmd){
		case bugcheck:
			write = false;
			break;
		case disk_read:
			write = true;
			break;
		case dbgbreak:
			int_trap(3);
			return 0;
		case halt:
			shutdown();
		case set_rw:
			return filesystem.set_rw(buffer) ? SUCCESS : FAILED;
		default:
			return BAD_PARAM;
	}
	if (!check(buffer,length,write))
		return BAD_BUFFER;
	
	switch(cmd){
		case bugcheck:
			bugcheck("%t",buffer,length);
			return FAILED;
		case disk_read:
		{
			if (length < SECTOR_SIZE)
				return TOO_SMALL;
			auto lba = *(qword*)buffer;
			auto slot = dm.get(lba,1,false);
			if (slot == nullptr)
				return FAILED;
			auto sor = slot->data(lba);
			assert(sor);
			memcpy(buffer,sor,SECTOR_SIZE);
			dm.relax(slot);
			return pack_qword(SUCCESS,1);
		}
	}
	bugcheck("unknown osctl %x",(qword)cmd);
}

qword service_provider::os_info(void* buffer,dword limit){
	if (!check(buffer,limit,true))
		return BAD_BUFFER;
	auto size = sizeof(OS_INFO) + sizeof(COFUOS_DESCRIPTION);
	if (limit < sizeof(OS_INFO))
		return pack_qword(TOO_SMALL,size);
	auto info = (OS_INFO*)buffer;
	info->version = COFUOS_VERSION;
	info->features = 0;	//TODO
	info->active_core = cores.size();
	//TODO info->cpu_load
	info->process_count = proc.size();
	info->running_time = timer.running_time();
	info->total_memory = PAGE_SIZE*pm.capacity();
	info->used_memory = info->total_memory - PAGE_SIZE*pm.available();
	info->resolution_x = display.get_width();
	info->resolution_y = display.get_height();
	info->reserved = 0;
	if (limit < size)
		return pack_qword(SUCCESS,sizeof(OS_INFO));
	memcpy(info + 1,COFUOS_DESCRIPTION,sizeof(COFUOS_DESCRIPTION));
	return pack_qword(SUCCESS,size);
}
qword service_provider::get_time(void){
	return rtc.get_time();
}
qword service_provider::enum_process(dword id){
	auto res = proc.enumerate(id);
	return pack_qword(res ? SUCCESS : FAILED,id);
}
STATUS service_provider::display_fill(dword color,qword val){
	if (this_process->get_privilege() > SHELL)
		return DENIED;
	rectangle rect;
	unpack_rect(rect,val);
	return display.fill(rect,color) ? SUCCESS : BAD_PARAM;
}
STATUS service_provider::display_draw(void const* buffer,qword val,word advance){
	if (this_process->get_privilege() > SHELL)
		return DENIED;
	rectangle rect;
	unpack_rect(rect,val);
	if (rect.right <= rect.left || rect.bottom <= rect.top || advance < (rect.right - rect.left))
		return BAD_PARAM;
	auto length = (rect.right - rect.left + advance*(rect.bottom - rect.top - 1) )*sizeof(dword);
	if (!check(buffer,length))
		return BAD_BUFFER;
	return display.draw(rect,(dword const*)buffer,advance) ? SUCCESS : BAD_PARAM;
}
HANDLE service_provider::get_thread(void){
	auto res = this_thread->acquire();
	assert(res);
	auto handle = this_process->handles.put(this_thread);
	if (!handle)
		this_thread->relax();
	return handle;
}
dword service_provider::thread_id(HANDLE handle){
	auto th = static_cast<thread*>(get(handle,OBJ_THREAD));
	if (th == nullptr)
		return 0;
	return th->id;
}
qword service_provider::get_handler(void){
	return this_thread->user_handler;
}
dword service_provider::get_priority(HANDLE handle){
	auto th = static_cast<thread*>(get(handle,OBJ_THREAD));
	if (th == nullptr)
		return BAD_HANDLE;
	return th->get_priority();
}
void service_provider::exit_thread(void){
	this_thread->drop();
	thread::kill(this_thread);
	bugcheck("exit_thread failed");
}
STATUS service_provider::kill_thread(HANDLE handle){
	do{
		auto th = static_cast<thread*>(get(handle,OBJ_THREAD));
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
	auto th = static_cast<thread*>(get(handle,OBJ_THREAD));
	if (th == nullptr)
		return BAD_HANDLE;
	if (val >= scheduler::idle_priority || val < scheduler::shell_priority)
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

	auto th = this_process->spawn(user_entry,args);
	if (th){
		HANDLE handle = this_process->handles.put(th);
		if (handle)
			return pack_qword(SUCCESS,handle);
	}
	th->relax();
	return NO_RESOURCE;
}
void service_provider::sleep(qword us){
	thread::sleep(us);
}
dword service_provider::wait_for(HANDLE handle,qword us,dword nowait){
	auto obj = get(handle);
	if (obj == nullptr)
		return BAD_HANDLE;
	if (nowait)
		return obj->check() ? 1 : 0;

	if (obj == this_thread)
		return FAILED;
	assert(hold_handle);
	hold_handle = false;
	skip_critical = true;
	return obj->wait(us,[](void){
		this_core core;
		auto this_thread = core.this_thread();
		auto& table = this_thread->get_process()->handles;
		assert(table.is_locked() && !table.is_exclusive());
		table.unlock();
		this_thread->drop();
	});
}
dword service_provider::signal(HANDLE handle,dword mode){
	auto obj = get(handle);
	if (obj == nullptr)
		return BAD_HANDLE;
	switch(obj->type()){
		case OBJ_SEMAPHORE:
			return static_cast<semaphore*>(obj)->signal();
		case OBJ_EVENT:
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
	auto res = this_process->acquire();
	assert(res);
	auto handle = this_process->handles.put(this_process);
	if (!handle)
		this_process->relax();
	return handle;
}
dword service_provider::process_id(HANDLE handle){
	auto ps = static_cast<process*>(get(handle,OBJ_PROCESS));
	if (ps == nullptr)
		return 0;
	return ps->id;
}
qword service_provider::process_info(HANDLE handle,void* buffer,dword limit){
	auto ps = static_cast<process*>(get(handle,OBJ_PROCESS));
	if (ps == nullptr)
		return BAD_HANDLE;
	if (!check(buffer,limit,true))
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
	auto ps = static_cast<process*>(get(handle,OBJ_PROCESS));
	if (ps == nullptr)
		return BAD_HANDLE;
	if (!check(buffer,limit,true))
		return BAD_BUFFER;
	const auto size = ps->commandline.size();
	if (limit < size)
		return pack_qword(TOO_SMALL,size);
	memcpy(buffer,ps->commandline.c_str(),size);
	return pack_qword(SUCCESS,size);
}
void service_provider::exit_process(dword result){
	this_thread->drop();
	this_process->kill(result);
	bugcheck("exit_process failed");
}
STATUS service_provider::kill_process(HANDLE handle,dword result){
	do{
		auto ps = static_cast<process*>(get(handle,OBJ_PROCESS));
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
	auto ps = static_cast<process*>(get(handle,OBJ_PROCESS));
	if (ps == nullptr)
		return BAD_HANDLE;
	dword result;
	if (!ps->get_result(result))
		return FAILED;
	return pack_qword(SUCCESS,result);
}
qword service_provider::create_process(void const* ptr,dword length){
	if (!check(ptr,length) || length < sizeof(STARTUP_INFO))
		return BAD_BUFFER;
	auto info = (const STARTUP_INFO*)ptr;
	if (info->cmd_length > PAGE_SIZE)
		return BAD_PARAM;
	if (!check(info->commandline,info->cmd_length) || info->cmd_length == 0)
		return BAD_BUFFER;

	process_manager::spawn_info ps_info;

	if (info->work_dir){
		if (info->wd_length > PAGE_SIZE)
			return BAD_PARAM;
		if (!check(info->work_dir,info->wd_length))
			return BAD_BUFFER;
		ps_info.work_dir = span<char>(info->work_dir,info->wd_length);
	}
	if (info->environment){
		if (info->env_length > PAGE_SIZE)
			return BAD_PARAM;
		if (!check(info->environment,info->env_length))
			return BAD_BUFFER;
		ps_info.env.assign(info->environment,info->environment + info->env_length);
	}
	if (info->flags && (PRIVILEGE)info->flags < this_process->get_privilege())
		return DENIED;

	ps_info.privilege = info->flags ? (PRIVILEGE)info->flags : this_process->get_privilege();

	for (auto i = 0;i < 3;++i){
		if (0 == info->std_handle[i]){
			ps_info.std_stream[i] = nullptr;
			continue;
		}
		auto st = get(info->std_handle[i],OBJ_STREAM);
		if (st == nullptr)
			return BAD_HANDLE;
		ps_info.std_stream[i] = static_cast<stream*>(st);
	}

	literal cmd(info->commandline,info->commandline + info->cmd_length);

	auto ps = proc.spawn(move(cmd),ps_info);
	if (ps){
		if (hold_handle){
			this_process->handles.unlock();
			hold_handle = false;
		}
		HANDLE handle = this_process->handles.put(ps);
		if (handle)
			return pack_qword(SUCCESS,handle);
		ps->relax();
		return NO_RESOURCE;
	}
	return NOT_FOUND;
}
qword service_provider::open_process(dword id){
	auto ps = proc.find(id,true);
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
qword service_provider::get_work_dir(void* buffer,dword length){
	if (!check(buffer,length,true))
		return BAD_BUFFER;
	auto wd = this_process->get_work_dir();
	if (wd){
		string str;
		wd->get_path(str);
		wd->relax();
		auto size = str.size();
		if (size > length)
			return pack_qword(TOO_SMALL,size);
		memcpy(buffer,str.c_str(),size);
		return pack_qword(SUCCESS,size);
	}
	return pack_qword(SUCCESS,0);
}
STATUS service_provider::set_work_dir(const void* buffer,dword length){
	if (!check(buffer,length))
		return BAD_BUFFER;
	if (length > PAGE_SIZE)
		return BAD_PARAM;
	return this_process->set_work_dir(span<char>((const char*)buffer,length)) ? SUCCESS : FAILED;
}
OBJTYPE service_provider::handle_type(HANDLE handle){
	auto obj = get(handle);
	if (obj == nullptr)
		return OBJ_UNKNOWN;
	return obj->type();
}
qword service_provider::open_handle(const void* buffer,dword length){
	if (!check(buffer,length) || length == 0)
		return BAD_BUFFER;
	span<char> name((char const*)buffer,length);

	waitable* ptr = nullptr;
	{
		lock_guard<object_manager> guard(named_obj);
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
	switch(type){
		case OBJ_SEMAPHORE:
			if (a1)
				ptr = new semaphore(a1);
			break;
		case OBJ_EVENT:
			ptr = new event(a1);
			break;
		case OBJ_PIPE:
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
	constexpr qword mask = 0x800000000000007B;
	auto pt = vspace->peek(va);
	return mask & *reinterpret_cast<qword const*>(&pt);
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
qword service_provider::stream_state(HANDLE handle){
	auto obj = get(handle,OBJ_STREAM);
	if (obj == nullptr)
		return BAD_HANDLE;
	auto ptr = static_cast<stream*>(obj);
	return pack_qword(ptr->state(),ptr->result());
}
qword service_provider::stream_read(HANDLE handle,void* buffer,dword limit){
	auto obj = get(handle,OBJ_STREAM);
	if (obj == nullptr)
		return BAD_HANDLE;
	auto f = static_cast<stream*>(obj);
	if (!check(buffer,limit,true))
		return BAD_BUFFER;
	assert(0 == ((qword)buffer & HIGHADDR(0)));
	auto len = f->read(buffer,limit);
	return pack_qword(SUCCESS,len);
}
qword service_provider::stream_write(HANDLE handle,void const* buffer,dword length){
	auto obj = get(handle,OBJ_STREAM);
	if (obj == nullptr)
		return BAD_HANDLE;
	auto f = static_cast<stream*>(obj);
	if (!check(buffer,length))
		return BAD_BUFFER;
	assert(0 == ((qword)buffer & HIGHADDR(0)));
	auto len = f->write(buffer,length);
	return pack_qword(SUCCESS,len);
}
qword service_provider::file_open(const void* name,dword length,dword mode){
	if (length > PAGE_SIZE)
		return BAD_PARAM;
	if (!check(name,length) || length == 0)
		return BAD_BUFFER;
	// if (access)	//not implemented
	// 	return BAD_PARAM;

	constexpr byte mask = ALLOW_FILE | ALLOW_FOLDER;
	byte allow = (mode & mask);
	auto f = file::open(span<char>((const char*)name,length),allow ? allow : mask);
	if (!f)
		return NOT_FOUND;
	auto handle = this_process->handles.put(f);
	if (handle)
		return pack_qword(SUCCESS,handle);
	f->relax();
	return NO_RESOURCE;
}
STATUS service_provider::file_tell(HANDLE handle,void* buffer){
	auto obj = get(handle,OBJ_FILE);
	if (obj == nullptr)
		return BAD_HANDLE;
	auto f = static_cast<file*>(obj);

	if (!check(buffer,2*sizeof(qword),true))
		return BAD_BUFFER;
	auto info = (qword*)buffer;
	info[0] = f->tell();
	info[1] = f->size();
	return SUCCESS;
}
STATUS service_provider::file_seek(HANDLE handle,qword offset,dword mode){
	auto obj = get(handle,OBJ_FILE);
	if (obj == nullptr)
		return BAD_HANDLE;
	auto f = static_cast<file*>(obj);
	if (mode)
		return BAD_PARAM;
	return f->seek(offset) ? SUCCESS : FAILED;
}
STATUS service_provider::file_setsize(HANDLE handle,qword new_size){
	bugcheck("file_setsize not implemented");
}
qword service_provider::file_path(HANDLE handle,void* buffer,dword length){
	if (!check(buffer,length,true))
		return BAD_BUFFER;
	auto obj = get(handle,OBJ_FILE);
	if (obj == nullptr)
		return BAD_HANDLE;
	auto f = static_cast<file*>(obj);
	auto str = f->get_path();
	auto size = str.size();
	if (size > length){
		return pack_qword(TOO_SMALL,size);
	}
	memcpy(buffer,str.c_str(),size);
	return pack_qword(SUCCESS,size);

}
qword service_provider::file_info(HANDLE handle,void* buffer,dword length){
	bugcheck("file_info not implemented");
}
STATUS service_provider::file_change(HANDLE handle,dword attrib){
	bugcheck("file_change not implemented");
}
STATUS service_provider::file_move(HANDLE handle,const void* target,dword length){
	bugcheck("file_move not implemented");
}