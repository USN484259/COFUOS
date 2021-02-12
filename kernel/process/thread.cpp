#include "thread.hpp"
#include "core_state.hpp"
#include "constant.hpp"
#include "memory/include/vm.hpp"
#include "image/include/pe.hpp"
#include "assert.hpp"
using namespace UOS;

qword atomic_id::operator()(void){
	do{
		auto cur = count;
		if (cur == cmpxchg(&count,cur + 1,cur)){
			return cur;
		}
	}while(true);
}

atomic_id thread::new_id;

//initial thread
thread::thread(initial_thread_tag, process& p) : id(new_id()), ps(p) {
	assert(id == 0);
	krnl_stk_top = 0;	//???
	krnl_stk_reserved = pe_kernel->stk_reserve;
}

thread::thread(process& p, procedure entry, void* arg, qword stk_size) : id(new_id()), ps(p), krnl_stk_reserved(stk_size) {
	auto va = vm.reserve(0,krnl_stk_reserved);
	if (!va)
		bugcheck("vm.reserve failed with 0x%x pages",krnl_stk_reserved);
	krnl_stk_top = va + krnl_stk_reserved;
	auto res = vm.commit(krnl_stk_top - PAGE_SIZE,1);
	if (!res)
		bugcheck("vm.commit failed @ %p",krnl_stk_top - PAGE_SIZE);
	gpr.rbp = krnl_stk_top;
	gpr.rsp = krnl_stk_top - 0x20;
	gpr.ss = SEG_KRNL_SS;
	gpr.cs = SEG_KRNL_CS;
	gpr.rip = reinterpret_cast<qword>(entry);
	gpr.rflags = 0x202;		//IF
	gpr.rcx = reinterpret_cast<qword>(arg);
	ready_queue.put(this);
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

process& thread::get_process(void){
	return ps;
}

void thread::set_state(thread::STATE st){
	state = st;
}
//TODO
/*
void thread::sleep(qword us){
	#error TODO
}
*/