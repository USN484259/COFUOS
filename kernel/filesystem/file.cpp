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
	lock_guard<file_instance> guard(*instance,rwlock::SHARED);
	return instance->get_size();
}

string file::get_path(void) const{
	string str;
	instance->get_path(str);
	return str;
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
			lock_or(&iostate,(word)OP_FAILURE);
			//iostate |= OP_FAILURE;
			return 0;
		}
		buffer = dst;
		length = len;
		command = instance->is_folder() ? COMMAND_LIST : COMMAND_READ;
		iostate = 0;
	}
	filesystem.task(this);
	return 0;
}

dword file::write(const void* sor,dword len){
	{
		interrupt_guard<spin_lock> guard(objlock);
		if (command || instance->is_folder()){
			lock_or(&iostate,(word)OP_FAILURE);
			//iostate |= OP_FAILURE;
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

file_instance* file::imp_open(folder_instance* cur,const span<char>& path,byte mode){
	assert(cur);
	// if (path.empty() && (mode & ALLOW_FOLDER)){
	// 	return cur;
	// }
	auto it = path.begin();
	while(true){
		if (it == path.end()){
			// got folder
			if (mode & ALLOW_FOLDER)
				return cur;
			break;
		}
		auto tail = it;
		while(tail != path.end() && *tail != '/')
			++tail;

		if (it == tail)
			break;
		file_instance* next;
		do{
			auto name = span<char>(it,tail);
			assert(!name.empty());
			if (name[0] == '.'){
				if (name.size() == 1){
					// this folder
					next = cur;
					// next->acquire();
					break;
				}
				if (name.size() == 2 && name[1] == '.'){
					// parent folder
					{
						lock_guard<file_instance> guard(*cur,rwlock::SHARED);
						next = cur->get_parent();
					}
					if (next)
						next->acquire();
					break;
				}
			}
			next = cur->open(name,tail == path.end() ? mode : ALLOW_FOLDER);
		}while(false);
		if (!next)
			break;
		
		if (tail == path.end()){
			// got file
			cur->relax();
			return next;
		}
		assert(next->is_folder());

		if (next != cur){	// skip "./"
			lock_guard<file_instance> guard(*next,rwlock::SHARED);
			cur->relax();
			cur = static_cast<folder_instance*>(next);
		}
		it = ++tail;
	}
	cur->relax();
	return nullptr;
}

file* file::open(const span<char>& path,byte mode){
	if (path.empty())
		return nullptr;
	this_core core;
	auto this_process = core.this_thread()->get_process();

	folder_instance* cur = nullptr;
	auto it = path.begin();
	if (*it == '/'){
		cur = filesystem.get_root();
		cur->acquire();
		++it;
	}
	else do{
		// mode 0x80 => load program
		if ((mode & 0x80) && find_first_of(path.begin(),path.end(),'/') == path.end()){
			auto bin = filesystem.get_root()->open(span<char>("bin",3),ALLOW_FOLDER);
			assert(bin->is_folder());
			cur = static_cast<folder_instance*>(bin);
			break;
		}
		// parse relative path
		cur = this_process->get_work_dir();
		if (cur)
			break;
		return nullptr;
	}while(false);

	assert(cur);
	auto ins = imp_open(cur,span<char>(it,path.end()),mode);
	if (ins){
		auto f = new file(ins,this_process);
		f->manage();
		return f;
	}
	return nullptr;
}

folder_instance* file::open_wd(folder_instance* cwd,const span<char>& path){
	assert(!path.empty());
	auto it = path.begin();
	if (*it == '/'){
		cwd = filesystem.get_root();
		++it;
	}
	else if (cwd == nullptr)
		return nullptr;
	
	assert(cwd);
	cwd->acquire();
	auto ins = imp_open(cwd,span<char>(it,path.end()),ALLOW_FOLDER);
	assert(!ins || ins->is_folder());
	return static_cast<folder_instance*>(ins);
}
