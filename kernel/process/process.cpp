#include "process.hpp"
#include "sync/include/lock_guard.hpp"
#include "memory/include/vm.hpp"
#include "intrinsics.hpp"
#include "core_state.hpp"
#include "dev/include/timer.hpp"

using namespace UOS;

id_gen<dword> process::new_id;

handle_table::handle_table(waitable* obj){
	assert(obj);
	table.push_back(obj);
	++count;
}

handle_table::~handle_table(void){
	interrupt_guard<rwlock> guard(objlock);
	if (count)
		bugcheck("deleting non-empty handle_table @ %p",this);
	//clear();
}

void handle_table::clear(void){
	interrupt_guard<rwlock> guard(objlock);
	for (auto ptr : table){
		if (ptr){
			assert(count);
			ptr->relax();
			--count;
		}
	}
	assert(count == 0);
}

dword handle_table::put(waitable* ptr){
	interrupt_guard<rwlock> guard(objlock);
	if (table.size()*4 >= count*3){
		table.push_back(ptr);
		++count;
		return table.size() - 1;
	}
	else{
		for (auto it = table.begin();it != table.end();++it){
			if (*it == nullptr){
				*it = ptr;
				++count;
				return it - table.begin();
			}
		}
		bugcheck("handle_table size mismatch @ %p (%x,%x)",this,count,table.size());
	}
}

bool handle_table::close(dword index){
	waitable* ptr;
	{
		interrupt_guard<rwlock> guard(objlock);
		if (index >= table.size())
			return false;
		auto& ref = table.at(index);
		if (ref == nullptr)
			return false;
		assert(count);
		ptr = ref;
		--count;
		ref = nullptr;
	}
	ptr->relax();
	return true;
}

waitable* handle_table::operator[](dword index) const{
	assert(objlock.is_locked() && !objlock.is_exclusive());
	if (index >= table.size())
		return nullptr;
	return table.at(index);
}

process::process(initial_process_tag) : id (new_id()), vspace(&vm), privilege(KERNEL), image(nullptr),\
	handles(&*threads.insert(thread::initial_thread_tag(), this)),start_time(0) { assert(id == 0); }

process::process(const string& cmd,basic_file* file,const qword* args) : \
	id(new_id()),vspace(new user_vspace()),commandline(cmd),\
	handles(file),start_time(timer.running_time())
{
	//image file handle as handle 0, user not accessible
	IF_assert;
	acquire();
	dbgprint("spawned new process $%d @ %p",id,this);
	HANDLE th = spawn(process_loader,args);
	assert(th);
	auto res = handles.close(th);
	assert(res);
}

process::~process(void){
	if (!threads.empty() || active_count){
		bugcheck("deleting non-stop process #%d (%d,%d) @ %p",id,active_count,threads.size(),this);
	}
	dbgprint("deleted process $%d @ %p",id,this);
	delete vspace;
}

HANDLE process::spawn(thread::procedure entry,const qword* args,qword stk_size){
	if (state == STOPPED)
		return 0;
	if (stk_size)
		stk_size = max(stk_size,PAGE_SIZE);
	else
		stk_size = pe_kernel->stk_reserve;

	HANDLE handle;
	thread* th;
	{
		interrupt_guard<spin_lock> guard(objlock);
		auto it = threads.insert(this,entry,args,stk_size);
		++active_count;
		th = &*it;
		handle = handles.put(th);
	}
	if (handle == 0)
		th->relax();
	return handle;
}

void process::kill(dword ret_val){
	this_core core;
	auto this_thread = core.this_thread();
	bool kill_self = false;
	{
		interrupt_guard<spin_lock> guard(objlock);
		state = STOPPED;
		result = ret_val;
		for (auto& th : threads){
			if (&th == this_thread){
				kill_self = true;
				continue;
			}
			thread::kill(&th);
		}
	}
	if (kill_self)
		thread::kill(this_thread);
}

void process::on_exit(void){
	{
		interrupt_guard<spin_lock> guard(objlock);
		assert(active_count && active_count <= threads.size());
		if (--active_count)
			return;
		state = STOPPED;
	}
	handles.clear();
	dbgprint("process $%d exit with %x",id,(qword)result);
	notify();
}

void process::erase(thread* th){
	assert(th && th->get_process() == this);
	{	
		interrupt_guard<spin_lock> guard(objlock);
		assert(active_count <= threads.size());	
		auto it = threads.find(th->id);
		if (it == threads.end()){
			bugcheck("cannot find thread %p in process %p",th,this);
		}
		threads.erase(it);
		if (!threads.empty())
			return;
		assert(active_count == 0);
	}
	relax();
}

REASON process::wait(qword us,wait_callback func){
	if (state == STOPPED){
		if (func)
			func();
		return PASSED;
	}
	return waitable::wait(us,func);
}

bool process::relax(void){
	auto res = waitable::relax();
	if (!res){
		proc.erase(this);
	}
	return res;
}

process_manager::process_manager(void){
	//create initial thread & process
	table.insert(process::initial_process_tag());
}

thread* process_manager::get_initial_thread(void){
	interrupt_guard<spin_lock> guard(lock);
	auto it = table.find(0);
	assert(it != table.end());
	return it->get_thread(0);
}

HANDLE process_manager::spawn(string&& command,string&& env){
	//TODO replace with real file-opening logic
	auto sep = command.find_first_of(' ');
	string filename(command.substr(command.begin(),sep));
	if (filename != "/test.exe")
		return 0;
	basic_file* file = new file_stub();
	//read & validate PE header
	byte buffer[0x200];
	if (0x200 != file->read(buffer,0x200))
		return 0;
	if (!file->seek(0))
		return 0;
	auto header = PE64::construct(buffer,0x200);
	if (header == nullptr || \
		(0 == (header->img_type & 0x02)) || \
		(header->img_type & 0x3000) || \
		IS_HIGHADDR(header->imgbase) || \
		(header->imgbase & PAGE_MASK) || \
		(header->align_section & PAGE_MASK) || \
		(header->align_file & 0x1FF) || \
		(header->header_size & 0x1FF) || \
		(0 == header->section_count))
			return 0;
	//(likely) valid image file:
	//non-system, non-dll, executable, valid vbase, valid alignment, has section
	
	qword info[4] = {
		reinterpret_cast<qword>(env.empty() ? nullptr : new string(move(env))),
		header->imgbase,
		header->imgsize,
		header->header_size
	};

	this_core core;
	auto this_process = core.this_thread()->get_process();
	HANDLE handle;
	process* ps;
	{
		//create a process and load this image
		interrupt_guard<spin_lock> guard(lock);
		auto it = table.insert(move(command),file,info);
		ps = &*it;
		handle = this_process->handles.put(ps);
	}
	if (handle == 0)
		ps->relax();
	return handle;
}

void process_manager::erase(process* ps){
	interrupt_guard<spin_lock> guard(lock);
	auto it = table.find(ps->id);
	if (it == table.end()){
		bugcheck("cannot find process %p",ps);
	}
	table.erase(it);
}

bool process_manager::enumerate(dword& id){
	interrupt_guard<spin_lock> guard(lock,spin_lock::SHARED);
	decltype(table)::iterator it;
	if (id == 0)
		it = table.begin();
	else{
		it = table.find(id);
		if (it == table.end())
			return false;
		++it;
	}
	while(it != table.end()){
		if (it->id != 0){
			id = it->id;
			return true;
		}
		++it;
	}
	id = 0;
	return true;
}

process* process_manager::get(dword id){
	interrupt_guard<spin_lock> guard(lock);
	auto it = table.find(id);
	if (it == table.end())
		return nullptr;
	if (!it->acquire())
		return nullptr;
	return &*it;
}