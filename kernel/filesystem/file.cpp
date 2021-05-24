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

file* file::duplicate(process* ps){
	interrupt_guard<spin_lock> guard(objlock);
	instance->acquire();
	auto ptr = new file(instance,ps);
	ptr->manage();
	return ptr;
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
	return length;
}

qword file::size(void) const{
	lock_guard<file_instance> guard(*instance);
	return instance->get_size();
}

bool file::seek(qword off){
	interrupt_guard<spin_lock> guard(objlock);
	if (command)
		return false;
	offset = off;
	return true;
}

dword file::read(void* dst,dword len){
	{
		interrupt_guard<spin_lock> guard(objlock);
		if (command){
			iostate |= OP_FAILURE;
			return 0;
		}
		buffer = dst;
		length = len;
		command = COMMAND_READ;
		iostate = 0;
	}
	filesystem.task(this);
	return 0;
}

dword file::write(const void* sor,dword len){
	{
		interrupt_guard<spin_lock> guard(objlock);
		if (command){
			iostate |= OP_FAILURE;
			return 0;
		}
		buffer = (void*)sor;
		length = len;
		command = COMMAND_WRITE;
		iostate = 0;
	}
	filesystem.task(this);
	return 0;
}

file* file::open(const span<char>& path,bool program){
	if (path.empty())
		return nullptr;
	folder_instance* cur = nullptr;
	auto it = path.begin();

	if (*it == '/'){
		cur = filesystem.get_root();
		cur->acquire();
		++it;
	}
	else{
		if (program && find_first_of(path.begin(),path.end(),'/') == path.end()){
			auto bin = filesystem.get_root()->open(span<char>("bin",3));
			if (bin->is_folder())
				cur = static_cast<folder_instance*>(bin);
		}
		if (!cur){
			// TODO parse relative path
			return nullptr;
		}
	}
	while(it != path.end()){
		auto tail = it;
		while(tail != path.end() && *tail != '/')
			++tail;
		file_instance* next;
		if (it == tail){
			//opening directory ending with '/'
			next = cur;
			cur = nullptr;
		}
		else{
			next = cur->open(span<char>(it,tail));
			if (!next)
				break;
		}
		if (tail == path.end()){
			// got file, create file object
			this_core core;
			auto this_process = core.this_thread()->get_process();
			auto f = new file(next,this_process);
			f->manage();
			if (cur)
				cur->relax();
			return f;
		}
		lock_guard<file_instance> guard(*next);
		if (!next->is_folder()){
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
