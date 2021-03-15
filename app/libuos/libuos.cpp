#include "libuos.h"
#include "kernel/interface/include/service_code.hpp"

using srv = UOS::service_code;

extern "C"
qword syscall(srv cmd,...);

template<typename T>
inline STATUS unpack_qword(qword val,T* res){
	*res = (T)(val >> 32);
	return (STATUS)(dword)val;
}
inline void pack_rect(const rectangle& rect,qword& lt,qword& rb){
	lt = ((qword)rect.top << 32) | rect.left;
	rb = ((qword)rect.bottom << 32) | rect.right;
}

STATUS os_info(OS_INFO* buffer,dword* length) {
	auto res = syscall(srv::os_info,buffer,*length);
	return unpack_qword(res,length);
}
qword get_time(void) {
	return syscall(srv::get_time);
}
STATUS enum_process(dword* id) {
	auto res = syscall(srv::enum_process,*id);
	return unpack_qword(res,id);
}
STATUS get_message(void* buffer,dword* length) {
	auto res = syscall(srv::get_message,buffer,*length);
	return unpack_qword(res,length);
}
void dbg_print(const char* str,dword length) {
	syscall(srv::dbg_print,str,length);
}
STATUS display_fill(dword color,const rectangle* rect) {
	qword lt,rb;
	pack_rect(*rect,lt,rb);
	return (STATUS)syscall(srv::display_fill,color,lt,rb);
}
STATUS display_draw(const dword* buffer,const rectangle* rect) {
	qword lt,rb;
	pack_rect(*rect,lt,rb);
	return (STATUS)syscall(srv::display_draw,buffer,lt,rb);
}
HANDLE get_thread(void) {
	return syscall(srv::get_thread);
}
STATUS thread_id(HANDLE handle,dword* id) {
	auto res = syscall(srv::thread_id,handle);
	return unpack_qword(res,id);
}
void* get_handler(void) {
	return (void*)syscall(srv::get_handler);
}
STATUS get_priority(HANDLE handle,byte* priority) {
	auto res = syscall(srv::get_priority,handle);
	return unpack_qword(res,priority);
}
void exit_thread(void) {
	syscall(srv::exit_thread);
}
STATUS kill_thread(HANDLE handle) {
	return (STATUS)syscall(srv::kill_thread,handle);
}
STATUS set_handler(void *func) {
	return (STATUS)syscall(srv::set_handler,func);
}
STATUS set_priority(HANDLE handle,byte priority) {
	return (STATUS)syscall(srv::set_priority,handle,priority);
}
STATUS create_thread(void* func,void* arg,dword stk_size,HANDLE* handle) {
	auto res = syscall(srv::create_thread,func,arg,stk_size);
	return unpack_qword(res,handle);
}
void sleep(qword us) {
	syscall(srv::sleep,us);
}
STATUS check(HANDLE handle,byte* state) {
	auto res = syscall(srv::check,handle);
	return unpack_qword(res,state);
}
STATUS wait_for(HANDLE handle,qword us,REASON* reason) {
	auto res = syscall(srv::wait_for,handle,us);
	return unpack_qword(res,reason);
}
HANDLE get_process(void) {
	return syscall(srv::get_process);
}
STATUS process_id(HANDLE handle,dword* id) {
	auto res = syscall(srv::process_id,handle);
	return unpack_qword(res,id);
}
STATUS process_info(HANDLE handle,PROCESS_INFO* buffer,dword* length) {
	auto res = syscall(srv::process_info,handle,buffer,*length);
	return unpack_qword(res,length);
}
STATUS get_command(HANDLE handle,char* buffer,dword* length) {
	auto res = syscall(srv::get_command,handle,buffer,*length);
	return unpack_qword(res,length);
}
void exit_process(dword result) {
	syscall(srv::exit_process,result);
}
STATUS kill_process(HANDLE handle,dword result) {
	return (STATUS)syscall(srv::kill_process,handle,result);
}
STATUS process_result(HANDLE handle,dword* result) {
	auto res = syscall(srv::process_result,handle);
	return unpack_qword(res,result);
}
STATUS create_process(const STARTUP_INFO* info,dword length,HANDLE* handle) {
	auto res = syscall(srv::create_process,info,length);
	return unpack_qword(res,handle);
}
STATUS open_process(dword id,HANDLE* handle) {
	auto res = syscall(srv::open_process,id);
	return unpack_qword(res,handle);
}
STATUS handle_type(HANDLE handle,OBJTYPE* type) {
	auto res = syscall(srv::handle_type,handle);
	return unpack_qword(res,type);
}
STATUS close_handle(HANDLE handle) {
	return (STATUS)syscall(srv::close_handle,handle);
}
qword vm_peek(void* va) {
	return syscall(srv::vm_peek,va);
}
STATUS vm_protect(void* base,dword count,qword attrib) {
	return (STATUS)syscall(srv::vm_protect,base,count,attrib);
}
void* vm_reserve(void* base,dword count) {
	return (void*)syscall(srv::vm_reserve,base,count);
}
STATUS vm_commit(void* base,dword count) {
	return (STATUS)syscall(srv::vm_commit,base,count);
}
STATUS vm_release(void* base,dword count) {
	return (STATUS)syscall(srv::vm_release,base,count);
}