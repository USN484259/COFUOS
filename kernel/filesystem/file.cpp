#include "file.hpp"
#include "lock_guard.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"

using namespace UOS;

file::file(file_instance* ins,process* ps) : instance(ins), host(ps) {}

file::~file(void){
	assert(command == 0 && next == nullptr);
	instance->relax();
}

bool file::relax(void){
	auto res = stream::relax();
	if (!res)
		delete this;
	return res;
}

bool file::check(void){
	return command;
}

REASON file::wait(qword us,wait_callback func){
	interrupt_guard<spin_lock> guard(objlock);
	if (func){
		func();
	}
	if (command == 0)
		return PASSED;
	guard.drop();
	return imp_wait(us);
}

dword file::result(void) const{
	interrupt_guard<spin_lock> guard(objlock);
	if (command)
		return 0;
	if (iostate & (IOSTATE::BAD | IOSTATE::FAIL))
		return 0;
	return length;
}

bool file::seek(size_t off){
	interrupt_guard<spin_lock> guard(objlock);
	if (command)
		return false;
	offset = off;
	return offset == off;
}

dword file::read(void* dst,dword len){
	{
		interrupt_guard<spin_lock> guard(objlock);
		if (command){
			iostate |= IOSTATE::FAIL;
			return 0;
		}
		buffer = dst;
		length = len;
		command = COMMAND_READ;
	}
	filesystem.task(this);
	return 0;
}

dword file::write(const void* sor,dword len){
	{
		interrupt_guard<spin_lock> guard(objlock);
		if (command){
			iostate |= IOSTATE::FAIL;
			return 0;
		}
		buffer = (void*)sor;
		length = len;
		command = COMMAND_WRITE;
	}
	filesystem.task(this);
	return 0;
}

file* file::duplicate(file* f,process* p){
	IF_assert;
	f->instance->acquire();
	auto ptr = new file(f->instance,p);
	ptr->manage();
	return ptr;
}

file* file::open(const span<char>& path){
	if (path.empty())
		return nullptr;
	folder_instance* cur;
	auto it = path.begin();

	if (*it == '/'){
		cur = filesystem.get_root();
		cur->acquire();
		++it;
	}
	else{
		// TODO parse relative path
		return nullptr;
	}
	while(it != path.end()){
		auto tail = it;
		while(tail != path.end() && *tail != '/')
			++tail;
		auto next = cur->open(span<char>(it,tail));
		if (!next)
			break;
		if (tail == path.end()){
			// got file, create file object
			this_core core;
			auto this_process = core.this_thread()->get_process();
			auto f = new file(next,this_process);
			f->manage();
			cur->relax();
			return f;
		}
		lock_guard<file_instance> guard(*next);
		if (0 == (next->get_attribute() & FOLDER)){
			// should be folder, but not folder
			next->relax();
			break;
		}
		cur->relax();
		cur = static_cast<folder_instance*>(next);
		it = ++tail;
	}
	cur->relax();
	return nullptr;
}
