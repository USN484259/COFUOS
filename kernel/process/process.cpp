#include "process.hpp"
#include "lock_guard.hpp"
#include "memory/include/vm.hpp"
#include "intrinsics.hpp"
#include "core_state.hpp"
#include "dev/include/timer.hpp"

using namespace UOS;

id_gen<dword> process::new_id;

handle_table::~handle_table(void){
	interrupt_guard<rwlock> guard(objlock);
	if (count)
		bugcheck("deleting non-empty handle_table @ %p",this);
	//clear();
	for (auto& ptr : table){
		if (ptr == nullptr)
			continue;
		auto res = vm.release((qword)ptr,1);
		if (!res)
			bugcheck("vm.release failed @ %p",ptr);
	}
}

void handle_table::clear(void){
	interrupt_guard<rwlock> guard(objlock);
	for (auto ptr : table){
		if (ptr){
			for (dword index = 0;index < handle_of_page;++index){
				if (ptr[index]){
					assert(count);
					ptr[index]->relax();
					ptr[index] = nullptr;
					--count;
				}
			}
		}
	}
	assert(count == 0);
}

dword handle_table::put(waitable* ptr){
	interrupt_guard<rwlock> guard(objlock);
	//interrupt_guard<rwlock> guard(objlock);
	auto index = avl_base;
	if (count*4 > top*3)
		index = max(index,top);
	
	while(index < limit){
		auto& page = table[index/handle_of_page];
		if (page == nullptr){
			auto va = vm.reserve(0,1);
			if (va && vm.commit(va,1)){
				page = (waitable**)va;
				zeromemory(page,PAGE_SIZE);
			}
			else{
				break;
			}
		}
		auto& slot = page[index%handle_of_page];
		if (slot){
			++index;
			continue;
		}
		slot = ptr;
		++count;
		top = max(top,index + 1);
		return index;
	}
	return 0;
}

bool handle_table::assign(dword index,waitable* ptr){
	if (index >= avl_base || ptr == nullptr)
		return false;
	{
		interrupt_guard<rwlock> guard(objlock);
		auto& page = table[index/handle_of_page];
		if (page == nullptr){
			auto va = vm.reserve(0,1);
			if (va && vm.commit(va,1)){
				page = (waitable**)va;
				zeromemory(page,PAGE_SIZE);
			}
			else{
				return false;
			}
		}
		bool has_obj = (ptr != nullptr);
		swap(page[index%handle_of_page],ptr);
		if (has_obj == (ptr == nullptr)){
			if (has_obj)
				++count;
			else{
				assert(count);
				--count;
			}
		}
	}
	if (ptr){
		ptr->relax();
	}
	return true;
}

bool handle_table::close(dword index){
	if (index >= limit)
		return false;
	waitable* ptr;
	{
		interrupt_guard<rwlock> guard(objlock);
		auto page = table[index/handle_of_page];
		if (page == nullptr)
			return false;
		auto& slot = page[index%handle_of_page];
		if (slot == nullptr)
			return false;
		assert(count);
		ptr = slot;
		--count;
		slot = nullptr;
		if (index + 1 >= top){
			--index;
			while(index >= avl_base){
				page = table[index/handle_of_page];
				if (page == nullptr)
					break;
				if (page[index%handle_of_page])
					break;
				--index;
			}
			top = index + 1;
		}
	}
	ptr->relax();
	return true;
}

waitable* handle_table::operator[](dword index) const{
	assert(objlock.is_locked() && !objlock.is_exclusive());
	if (index >= limit)
		return nullptr;
	auto page = table[index/handle_of_page];
	if (page == nullptr)
		return nullptr;
	return page[index%handle_of_page];
}

process::process(initial_process_tag) : id (new_id()), vspace(&vm),\
	privilege(KERNEL),active_count(1),image(nullptr),commandline("COFUOS"), start_time(0)
{
	assert(id == 0);
	auto it = threads.insert(thread::initial_thread_tag(),this);
	it->manage();
}

process::process(literal&& cmd,const spawn_info& info) : \
	id(new_id()),vspace(new user_vspace()),privilege(info.privilege),\
	work_dir(info.work_dir), commandline(move(cmd)), start_time(timer.running_time())
{
	IF_assert;
	//image file handle as handle 0, user not accessible
	auto res = handles.assign(0,info.f->duplicate(this));
	assert(res);

	//stream handles
	for (unsigned i = 0;i < 3;++i){
		auto st = info.std_stream[i];
		if (st){
			//st->acquire();
			handles.assign(i + 1,st->duplicate(this));
		}
	}
#ifdef PS_TEST
	dbgprint("spawned new process $%d @ %p",id,this);
#endif
	auto th = spawn(process_loader,&info.env_ptr);
	assert(th);
	th->relax();
}

process::~process(void){
	if (!threads.empty() || active_count){
		bugcheck("deleting non-stop process #%d (%d,%d) @ %p",id,active_count,threads.size(),this);
	}
	assert(work_dir == nullptr);
#ifdef PS_TEST
	dbgprint("deleted process $%d @ %p",id,this);
#endif
	delete vspace;
}

thread* process::spawn(thread::procedure entry,const qword* args,qword stk_size){
	if (state == STOPPED)
		return nullptr;
	if (stk_size)
		stk_size = max(stk_size,PAGE_SIZE);
	else
		stk_size = pe_kernel->stk_reserve;

	interrupt_guard<spin_lock> guard(objlock);
	auto it = threads.insert(this,entry,args,stk_size);
	it->manage();
	++active_count;
	return &*it;
}

void process::kill(dword ret_val){
#ifdef PS_TEST
	dbgprint("killing process %d @ %p",id,this);
#endif
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
	folder_instance* wd = nullptr;
	{
		interrupt_guard<spin_lock> guard(objlock);
		assert(active_count && active_count <= threads.size());
		if (--active_count)
			return;
		state = STOPPED;
		guard.drop();
		notify();
		wd = work_dir;
		work_dir = nullptr;
	}
#ifdef PS_TEST
	dbgprint("process $%d exit with %x",id,(qword)result);
#endif
	handles.clear();
	if (wd)
		wd->relax();
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

thread* process::find(dword tid,bool acq){
	interrupt_guard<spin_lock> guard(objlock);
	auto it = threads.find(tid);
	if (it == threads.end())
		return nullptr;
	if (acq && !it->acquire())
		return nullptr;
	return &(*it);
}

REASON process::wait(qword us,wait_callback func){
	if (state == STOPPED){
		if (func){
			func();
		}
		return PASSED;
	}
	return waitable::wait(us,func);
}

bool process::relax(void){
	interrupt_guard<void> ig;
	auto res = waitable::relax();
	if (!res){
		proc.erase(this);
	}
	return res;
}

void process::manage(void*){
	IF_assert;
	waitable::manage();
	++ref_count;
	assert(ref_count == 2);
}

bool process::set_work_dir(const span<char>& str){
	lock_guard<spin_lock> guard(objlock);
	if (str.empty()){
		if (work_dir){
			work_dir->relax();
			work_dir = nullptr;
		}
		return true;
	}
	auto wd = file::open_wd(work_dir,str);
	if (wd){
		if (work_dir)
			work_dir->relax();
		work_dir = wd;
		return true;
	}
	return false;
}

folder_instance* process::get_work_dir(void) const{
	lock_guard<spin_lock> guard(objlock);
	if (work_dir)
		work_dir->acquire();
	return work_dir;
}

process_manager::process_manager(void){
	//create initial thread & process
	auto it = table.insert(process::initial_process_tag());
	it->manage();
}
/*
thread* process_manager::get_initial_thread(void){
	interrupt_guard<spin_lock> guard(lock);
	auto it = table.find(0);
	assert(it != table.end());
	return it->get_thread(0);
}
*/
process* process_manager::spawn(literal&& command,spawn_info& info){
	assert(!command.empty());

	this_core core;
	auto this_process = core.this_thread()->get_process();
	
	file* f = nullptr;
	do{
		if (info.privilege < this_process->get_privilege())
			break;
		auto sep = find_first_of(command.begin(),command.end(),' ');
		span<char> filename(command.begin(),sep);
		f = file::open(filename,0x80 | ALLOW_FILE);	//0x80 => load program
		if (!f)
			break;

		//read & validate PE header
		byte buffer[SECTOR_SIZE];
		f->read(buffer,SECTOR_SIZE);
		f->wait();

		if (f->state() != 0 || SECTOR_SIZE != f->result())
			break;
		// if (!f->seek(0))
		// 	break;
		auto header = PE64::construct(buffer,SECTOR_SIZE);
		if (header == nullptr || \
			(0 == (header->img_type & 0x02)) || \
			(header->img_type & 0x3000) || \
			(header->imgbase & HIGHADDR(0)) || \
			(header->imgbase & PAGE_MASK) || \
			(header->align_section & PAGE_MASK) || \
			(header->align_file & 0x1FF) || \
			(header->header_size & 0x1FF) || \
			(0 == header->section_count))
				break;
		//(likely) valid image file:
		//non-system, non-dll, executable, valid vbase, valid alignment, has section

		process::spawn_info ps_info = {
			f,
			{
				info.std_stream[0],
				info.std_stream[1],
				info.std_stream[2]
			},
			info.work_dir.empty() ? this_process->get_work_dir() : file::open_wd(nullptr,info.work_dir),
			info.privilege,
			reinterpret_cast<qword>(info.env.detach()),
			header->imgbase,
			header->imgsize,
			header->header_size
		};

		if (command.at(0) != '/'){
			// replace with absolute path
			auto path = f->get_path();
			path.append(sep,command.end());
			command.assign(path.begin(),path.end());
		}

		//create a process and load this image
		interrupt_guard<spin_lock> guard(lock);
		auto it = table.insert(move(command),ps_info);
		it->manage();
		f->relax();
		return &*it;
	}while(false);
	
	if (f)
		f->relax();

	return nullptr;
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

process* process_manager::find(dword id,bool acq){
	interrupt_guard<spin_lock> guard(lock);
	auto it = table.find(id);
	if (it == table.end())
		return nullptr;
	if (acq && !it->acquire())
		return nullptr;
	return &(*it);
}