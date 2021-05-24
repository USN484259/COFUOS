#include "service.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"
#include "process/include/thread.hpp"
#include "dev/include/cpu.hpp"
#include "lock_guard.hpp"
#include "interface.h"
#include "assert.hpp"
#include "string.hpp"
#include "service_code.hpp"

using namespace UOS;

bool UOS::user_exception(qword& rip,qword& rsp,dword errcode){
	this_core core;
	auto this_thread = core.this_thread();
	auto vspace = this_thread->get_process()->vspace;
	assert(vspace != &vm);
	if (!this_thread->user_handler || (this_thread->user_handler & HIGHADDR(0))){
		return false;
	}
	interrupt_guard<virtual_space> guard(*vspace);
	auto aligned_rsp = align_down(rsp,8) - 0x10;
	auto pt = vspace->peek(aligned_rsp);
	if (!pt.present || !pt.write || !pt.user)
		return false;
	if (align_down(aligned_rsp + 0x20,PAGE_SIZE) != aligned_rsp){
		pt = vspace->peek(aligned_rsp + 0x08);
		if (!pt.present || !pt.write || !pt.user)
			return false;
	}
	//pass exception to user space
	auto stk_ptr = (qword*)aligned_rsp;
	stk_ptr[0] = rip;
	stk_ptr[1] = errcode;
	rip = this_thread->user_handler;
	rsp = aligned_rsp;
	this_thread->user_handler = 0;
	return true;
}

void UOS::process_loader(qword ptr,qword image_base,qword image_size,qword header_size){
	literal env;
	env.attach(reinterpret_cast<void*>(ptr));
	this_core core;
	thread* this_thread = core.this_thread();
	process* this_process = this_thread->get_process();
	file* f;
	{
		lock_guard<handle_table> guard(this_process->handles);
		auto obj = this_process->handles[0];
		assert(obj && obj->type() == OBJ_FILE);
		f = static_cast<file*>(obj);
	}
	auto res = f->seek(0);
	assert(res && f->tell() == 0 && f->state() == 0);
	do{
		
		//reserve image region
		auto image_page_count = align_up(image_size,PAGE_SIZE)/PAGE_SIZE;
		if (image_page_count == 0)
			break;
		if (image_base != this_process->vspace->reserve(image_base,image_page_count))
			break;
		//read full PE header
		if (!this_process->vspace->commit(image_base,align_up(header_size,PAGE_SIZE)/PAGE_SIZE))
			break;
		f->read((void*)image_base,header_size);
		f->wait();
		if (f->state() != 0 || f->result() != header_size)
			break;
		this_process->image = PE64::construct((void*)image_base,header_size);
		if (this_process->image == nullptr)
			break;
		assert(this_process->image->imgbase == image_base);
		assert(this_process->image->imgsize == image_size);
		assert(this_process->image->header_size == header_size);
		//prepare user stack
		auto stack_page_count = align_up(this_process->image->stk_reserve,PAGE_SIZE)/PAGE_SIZE;
		if (stack_page_count == 0)
			break;
		auto stack_base = this_process->vspace->reserve(0,1 + stack_page_count);
		if (stack_base == 0)
			break;
		if (!this_process->vspace->commit(stack_base + stack_page_count*PAGE_SIZE,1))
			break;
		this_thread->user_stk_reserved = stack_page_count*PAGE_SIZE;
		this_thread->user_stk_top = stack_base + (1 + stack_page_count)*PAGE_SIZE;

		//loads every section
		unsigned index = 0;
		do{
			auto section = this_process->image->get_section(index);
			if (section == nullptr)
				break;
			if (section->offset & PAGE_MASK)
				break;
			if ((section->fileoffset & 0x1FF) || (section->datasize & 0x1FF))
				break;
			auto section_page_count = align_up(max(section->datasize,section->size),PAGE_SIZE)/PAGE_SIZE;
			if (section_page_count == 0)
				continue;
			if (section->offset/PAGE_SIZE + section_page_count > image_page_count)
				break;
			auto section_base = image_base + section->offset;
			assert(0 == (section_base & PAGE_MASK));
			if (!this_process->vspace->commit(section_base,section_page_count))
				break;
			if (section->datasize){
				if (!f->seek(section->fileoffset))
					break;
				if (f->state() != 0 || f->tell() != section->fileoffset)
					break;
				f->read((void*)section_base,section->datasize);
				f->wait();
				if (f->state() != 0 || f->result() != section->datasize)
					break;
			}
			zeromemory((void*)(section_base + section->datasize),section_page_count*PAGE_SIZE - section->datasize);
			qword attrib = 0;
			if (section->attrib & 0x80000000)
				attrib |= PAGE_WRITE;
			if (0 == (section->attrib & 0x20000000))
				attrib |= PAGE_XD;
			//ignore read attribute
			if (!this_process->vspace->protect(section_base,section_page_count,attrib))
				break;
		}while(++index);
		if (index != this_process->image->section_count)
			break;
		
		//copy environment to user space
		qword env_page = 0;
		if (!env.empty()){
			auto env_count = align_up(env.size() + 1,PAGE_SIZE)/PAGE_SIZE;
			env_page = this_process->vspace->reserve(0,env_count);
			if (env_page == 0)
				break;
			if (!this_process->vspace->commit(env_page,env_count))
				break;
			memcpy((void*)env_page,env.c_str(),env.size() + 1);
			//no destructor called, release manually
			env.clear();
		}

		//TODO resolve import table
		
		//lower thread priority
		this_thread->set_priority(scheduler::user_priority);
		//make up user stack & 'return' to Ring3
		//WARNING destructor of local variable not called
		auto entrypoint = this_process->image->imgbase + this_process->image->entrance;
		if (entrypoint & HIGHADDR(0))
			break;
		service_exit(entrypoint,this_process->image->imgbase,env_page,this_thread->user_stk_top);
	}while(false);
	//no destructor called, release manually
	env.clear();
	//all user resource should be released on destruction
	thread::kill(this_thread);
	bugcheck("process_loader failed to exit");
}

void UOS::user_entry(qword entry,qword arg,qword stk_top,qword stk_size){
	this_core core;
	auto this_thread = core.this_thread();

	this_thread->user_stk_top = stk_top;
	this_thread->user_stk_reserved = stk_size;

	if (entry & HIGHADDR(0)){
		thread::kill(this_thread);
		bugcheck("user_entry failed to exit");
	}

	this_thread->set_priority(scheduler::user_priority);
	service_exit(entry,arg,0,stk_top);
}

extern "C"
qword kernel_service(dword cmd,qword a1,qword a2,qword a3,qword rip,qword rsp){
	service_provider srv;
	//dbgprint("%d:%d\t%x",srv.this_process->id,srv.this_thread->id,(qword)cmd);
	switch(cmd){
		case osctl:
			return srv.osctl((osctl_code)a1,(void*)a2,a3);
		case os_info:
			return srv.os_info((void*)a1,a2);
		case get_time:
			return srv.get_time();
		case enum_process:
			return srv.enum_process(a1);
		case display_fill:
			return srv.display_fill(a1,a2);
		case display_draw:
			return srv.display_draw((void const*)a1,a2,a3);
		case get_thread:
			return srv.get_thread();
		case thread_id:
			return srv.thread_id(a1);
		case get_handler:
			return srv.get_handler();
		case get_priority:
			return srv.get_priority(a1);
		case exit_thread:
			srv.exit_thread();
		case kill_thread:
			return srv.kill_thread(a1);
		case set_handler:
			return srv.set_handler(a1);
		case set_priority:
			return srv.set_priority(a1,a2);
		case create_thread:
			return srv.create_thread(a1,a2,a3);
		case sleep:
			srv.sleep(a1);
			return 0;
		case check:
			return srv.check(a1);
		case wait_for:
			return srv.wait_for(a1,a2);
		case signal:
			return srv.signal(a1,a2);
		case get_process:
			return srv.get_process();
		case process_id:
			return srv.process_id(a1);
		case process_info:
			return srv.process_info(a1,(void*)a2,a3);
		case get_command:
			return srv.get_command(a1,(void*)a2,a3);
		case exit_process:
			srv.exit_process(a1);
		case kill_process:
			return srv.kill_process(a1,a2);
		case process_result:
			return srv.process_result(a1);
		case create_process:
			return srv.create_process((void const*)a1,a2);
		case open_process:
			return srv.open_process(a1);
		case get_work_dir:
			return srv.get_work_dir((void*)a1,a2);
		case set_work_dir:
			return srv.set_work_dir((void const*)a1,a2);
		case handle_type:
			return srv.handle_type(a1);
		case open_handle:
			return srv.open_handle((void const*)a1,a2);
		case close_handle:
			return srv.close_handle(a1);
		case create_object:
			return srv.create_object((OBJTYPE)a1,a2,a3);
		case vm_peek:
			return srv.vm_peek(a1);
		case vm_protect:
			return srv.vm_protect(a1,a2,a3);
		case vm_reserve:
			return srv.vm_reserve(a1,a2);
		case vm_commit:
			return srv.vm_commit(a1,a2);
		case vm_release:
			return srv.vm_release(a1,a2);
		case stream_state:
			return srv.stream_state(a1);
		case stream_read:
			return srv.stream_read(a1,(void*)a2,a3);
		case stream_write:
			return srv.stream_write(a1,(void const*)a2,a3);
		case file_open:
			return srv.file_open((void const*)a1,a2,a3);
		case file_tell:
			return srv.file_tell(a1,(void*)a2);
		case file_seek:
			return srv.file_seek(a1,a2,a3);
		case file_setsize:
			return srv.file_setsize(a1,a2);
		case file_path:
			return srv.file_path(a1,(void*)a2,a3);
		case file_info:
			return srv.file_info(a1,(void*)a2,a3);
		case file_change:
			return srv.file_change(a1,a2);
		case file_move:
			return srv.file_move(a1,(void const*)a2,a3);
	}
	if (!user_exception(rip,rsp,ERROR_CODE::SV))
		srv.exit_process(ERROR_CODE::SV);
	return 0;
}