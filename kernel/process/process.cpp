#include "process.hpp"
#include "intrinsics.hpp"

using namespace UOS;

process::process(initial_process_tag) : id (initial_pid), cr3(read_cr3()) {
	threads.insert(thread::initial_thread_tag(), *this);
}

process_manager::process_manager(void){
	//create initial thread & process
	table.insert(process::initial_process_tag());
}

thread* process_manager::get_initial_thread(void){
	auto it = table.find(initial_pid);
	assert(it != table.end());
	return it->get_thread(0);
}

