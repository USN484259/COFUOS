#include "process/include/core_state.hpp"
#include "process/include/process.hpp"
#include "process/include/thread.hpp"
#include "hal.hpp"
#include "assert.hpp"
#include "string.hpp"

using namespace UOS;

void UOS::userentry(void* ptr){
	process::startup_info* info = (process::startup_info*)ptr;
	assert(info);
	this_core core;
	thread* this_thread = core.this_thread();
	process* self = this_thread->get_process();

	do{
		//image file handle as handle 0, user not accessible
		if (0 != self->handles.add(info->file))
			break;
		//reserve image region
		auto image_page_count = align_up(info->image_size,PAGE_SIZE)/PAGE_SIZE;
		if (image_page_count == 0)
			break;
		if (info->image_base != self->vspace->reserve(info->image_base,image_page_count))
			break;
		//read full PE header
		if (!self->vspace->commit(info->image_base,align_up(info->header_size,PAGE_SIZE)/PAGE_SIZE))
			break;
		assert(info->file->tell() == 0);
		if (info->header_size != info->file->read((void*)info->image_base,info->header_size))
			break;
		self->image = PE64::construct((void*)info->image_base,info->header_size);
		if (self->image == nullptr)
			break;
		assert(self->image->imgbase == info->image_base);
		assert(self->image->imgsize == info->image_size);
		assert(self->image->header_size == info->header_size);
		//prepare user stack
		auto stack_page_count = align_up(self->image->stk_reserve,PAGE_SIZE)/PAGE_SIZE;
		auto commit_page_count = max<qword>(1,self->image->stk_commit/PAGE_SIZE);
		if (stack_page_count == 0 || stack_page_count < commit_page_count)
			break;
		auto stack_base = self->vspace->reserve(0,stack_page_count);
		if (stack_base == 0)
			break;
		if (!self->vspace->commit(stack_base + (stack_page_count - commit_page_count)*PAGE_SIZE,commit_page_count))
			break;
		this_thread->user_stk_reserved = stack_page_count*PAGE_SIZE;
		this_thread->user_stk_top = stack_base + stack_page_count*PAGE_SIZE;

		//copy command string to new page
		assert(info->cmd_length <= 4000);
		auto cmd_page = self->vspace->reserve(0,1);
		if (cmd_page == 0)
			break;
		if (!self->vspace->commit(cmd_page,1))
			break;
		memcpy((void*)cmd_page,info + 1,info->cmd_length);

		//loads every section
		unsigned index = 0;
		do{
			auto section = self->image->get_section(index);
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
			auto section_base = info->image_base + section->offset;
			assert(0 == (section_base & PAGE_MASK));
			if (!self->vspace->commit(section_base,section_page_count))
				break;
			if (section->datasize){
				if (!info->file->seek(section->fileoffset))
					break;
				assert(info->file->tell() == section->fileoffset);
				if (section->datasize != info->file->read((void*)section_base,section->datasize))
					break;
			}
			zeromemory((void*)(section_base + section->datasize),section_page_count*PAGE_SIZE - section->datasize);
			qword attrib = 0;
			if (section->attrib & 0x80000000)
				attrib |= PAGE_WRITE;
			if (0 == (section->attrib & 0x20000000))
				attrib |= PAGE_XD;
			//ignore read attribute
			if (!self->vspace->protect(section_base,section_page_count,attrib))
				break;
		}while(++index);
		if (index != self->image->section_count)
			break;
		//release info block
		if (!vm.release((qword)ptr,1))
			bugcheck("vm.release failed @ %p",ptr);
		ptr = nullptr;
		info = nullptr;
		//TODO resolve import table
		
		//lower thread priority
		this_thread->set_priority(scheduler::user_priority);
		//make up user stack & 'return' to Ring3
		//WARNING destructor of local variable not called
		auto entrypoint = self->image->imgbase + self->image->entrance;
		service_exit(entrypoint,self->image->imgbase,cmd_page,this_thread->user_stk_top);
		bugcheck("switch to user mode failed");
	}while(false);
	if (ptr){
		assert(info);
		auto res = vm.release((qword)ptr,1);
		if (!res)
			bugcheck("vm.release failed @ %p",ptr);
	}
	else{
		assert(info == nullptr);
	}
	//all user resource should be released on destruction
	thread::exit();
}

