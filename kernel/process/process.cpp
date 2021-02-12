#include "process.hpp"
#include "sync/include/lock_guard.hpp"
#include "intrinsics.hpp"

using namespace UOS;

process::process(initial_process_tag) : id (initial_pid), cr3(read_cr3()), image(nullptr){
	threads.insert(thread::initial_thread_tag(), *this);
}

thread* process::spawn(thread::procedure entry,void* arg,qword stk_size){
	interrupt_guard<spin_lock> guard(lock);
	if (stk_size)
		stk_size = max(stk_size,PAGE_SIZE);
	else
		stk_size = pe_kernel->stk_reserve;
	auto it = threads.insert(*this,entry,arg,stk_size);
	return &*it;
}

process_manager::process_manager(void){
	//create initial thread & process
	table.insert(process::initial_process_tag());
}

thread* process_manager::get_initial_thread(void){
	interrupt_guard<spin_lock> guard(lock);
	auto it = table.find(initial_pid);
	assert(it != table.end());
	return it->get_thread(0);
}
