#include "uos.h"
#include "util.hpp"
#include "crt_heap.hpp"

using namespace UOS;

mutex::mutex(void){
	if (SUCCESS != create_object(SEMAPHORE,1,0,&semaphore))
		abort();
}

void mutex::lock(void){
	wait_for(semaphore,0);
}

bool mutex::try_lock(void){
	return (byte)check(semaphore);
}

void mutex::unlock(void){
	signal(semaphore,0);
}

UOS::buddy_heap<4,12,mutex> heap([](size_t& req_size) -> void* {
	req_size = align_up(max(req_size,PAGE_SIZE),PAGE_SIZE);
	do{
		auto req_page = req_size / PAGE_SIZE;
		auto ptr = vm_reserve(0,req_page);
		if (ptr){
			if (SUCCESS == vm_commit(ptr,req_page)){
				return (void*)ptr;
			}
			vm_release(ptr,req_page);
		}
		if (req_page <= 1)
			return nullptr;
		req_size = PAGE_SIZE;
	}while(true);
});