#include "thread.hpp"
#include "image/include/pe.hpp"

using namespace UOS;

//initial thread
thread::thread(initial_thread_tag, process& p) : id(0), ps(p) {
	
	krnl_stk_top = 0;	//???
	krnl_stk_reserved = pe_kernel->stk_reserve;
}

bool thread::has_context(void) const{
	return gpr.rflags != 0;
}

word thread::get_priority(void) const{
	return priority;
}

context const* thread::get_context(void) const{
	return &gpr;
}

void thread::set_state(thread::STATE st){
	state = st;
}