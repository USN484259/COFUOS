#include "process.hpp"
#include "sync/include/lock_guard.hpp"
#include "memory/include/vm.hpp"
#include "interface/include/loader.hpp"
#include "intrinsics.hpp"

using namespace UOS;

id_gen<dword> process::new_id;

handle_table::~handle_table(void){
	rwlock.lock();
	for (auto ptr : table){
		if (ptr){
			assert(count);
			ptr->relax();
			--count;
		}
	}
	assert(count == 0);
}

dword handle_table::add(waitable* ptr){
	interrupt_guard<spin_lock> guard(rwlock);
	if (table.size() == count){
		table.push_back(ptr);
		return count++;
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

waitable* handle_table::erase(dword index){
	interrupt_guard<spin_lock> guard(rwlock);
	if (index >= table.size())
		return nullptr;
	auto& ref = table.at(index);
	if (ref == nullptr)
		return nullptr;
	assert(count);
	auto ptr = ref;
	--count;
	ref = nullptr;
	return ptr;
}

waitable* handle_table::operator[](dword index){
	assert(rwlock.is_locked() && !rwlock.is_exclusive());
	if (index >= table.size())
		return nullptr;
	return table.at(index);
}

process::process(initial_process_tag, kernel_vspace* v) : id (new_id()), vspace(v), image(nullptr){
	assert(id == 0);
	threads.insert(thread::initial_thread_tag(), this);
}

process::process(startup_info* info) : id(new_id()),vspace(new user_vspace()){
	//WARNING: header may be incomplete
	assert(info && info->file && info->image_base && info->image_size && info->header_size && info->cmd_length);
	spawn(userentry,info);
}

process::~process(void){
	if (!threads.empty()){
		bugcheck("deleting non-stop process #%d @ %p",id,this);
	}
	delete vspace;
}

thread* process::spawn(thread::procedure entry,void* arg,qword stk_size){
	interrupt_guard<spin_lock> guard(lock);
	if (stk_size)
		stk_size = max(stk_size,PAGE_SIZE);
	else
		stk_size = pe_kernel->stk_reserve;
	auto it = threads.insert(this,entry,arg,stk_size);
	return &*it;
}

void process::erase(thread* th){
	if (th->get_state() != thread::STOPPED){
		bugcheck("killing other thread not implemented");
	}
	{
		interrupt_guard<spin_lock> guard(lock);
		auto it = threads.find(th->id);
		if (it == threads.end()){
			bugcheck("cannot find thread %p in process %p",th,this);
		}
		threads.erase(it);
		if (!threads.empty())
			return;
	}
	relax();
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
	table.insert(process::initial_process_tag(), &vm);
}

thread* process_manager::get_initial_thread(void){
	interrupt_guard<spin_lock> guard(lock);
	auto it = table.find(0);
	assert(it != table.end());
	return it->get_thread(0);
}

process* process_manager::spawn(const string& command){
	if (command.size() >= 4000)	//too long to fit into one page
		return nullptr;
	//TODO replace with real file-opening logic
	auto sep = command.find_first_of(' ');
	string filename(command.substr(command.begin(),sep));
	if (filename != "/test.exe")
		return nullptr;
	basic_file* file = new file_stub();
	//read & validate PE header
	byte buffer[0x200];
	if (0x200 != file->read(buffer,0x200))
		return nullptr;
	if (!file->seek(0))
		return nullptr;
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
			return nullptr;
	//(likely) valid image file:
	//non-system, non-dll, executable, valid vbase, valid alignment, has section
	//allocate one kernel page as startup-info
	//released after image loading
	qword info_page = vm.reserve(0,1);
	if (info_page == 0)
		return nullptr;
	auto res = vm.commit(info_page,1);
	if (!res){
		vm.release(info_page,1);
		return nullptr;
	}
	auto info = (process::startup_info*)(void*)(info_page);
	info->file = file;
	info->image_base = header->imgbase;
	info->image_size = header->imgsize;
	info->header_size = header->header_size;
	info->cmd_length = command.size() + 1;
	memcpy(info + 1,command.c_str(),command.size() + 1);	//also copys '\0'
	//create a process and load this image
	interrupt_guard<spin_lock> guard(lock);
	auto it = table.insert(info);
	return &*it;
}

void process_manager::erase(process* ps){
	interrupt_guard<spin_lock> guard(lock);
	auto it = table.find(ps->id);
	if (it == table.end()){
		bugcheck("cannot find process %p",ps);
	}
	table.erase(it);
}