#include "libuos.h"
#include "kernel/interface/include/service_code.hpp"

using srv = UOS::service_code;

extern "C"
qword syscall(srv cmd,...);

template<typename T,typename S = STATUS>
inline S unpack_qword(qword val,T* res){
	*res = (T)(val >> 32);
	return (S)(dword)val;
}
inline qword pack_rect(const rectangle& rect){ 
	dword lt = ((dword)rect.top << 16) | rect.left;
	dword rb = ((dword)rect.bottom << 16) | rect.right;
	return ((qword)rb << 32) | lt;
}

STATUS osctl(osctl_code cmd,void* buffer,dword* length){
	auto res = syscall(srv::osctl,cmd,buffer,*length);
	return unpack_qword(res,length);
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
STATUS display_fill(dword color,const rectangle* rect) {
	return (STATUS)syscall(srv::display_fill,color,pack_rect(*rect));
}
STATUS display_draw(const dword* buffer,const rectangle* rect,word advance) {
	return (STATUS)syscall(srv::display_draw,buffer,pack_rect(*rect),advance);
}
HANDLE get_thread(void) {
	return syscall(srv::get_thread);
}
dword thread_id(HANDLE handle) {
	return syscall(srv::thread_id,handle);
}
void* get_handler(void) {
	return (void*)syscall(srv::get_handler);
}
dword get_priority(HANDLE handle) {
	return syscall(srv::get_priority,handle);
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
STATUS create_thread(void (*func)(void*,void*),void* arg,dword stk_size,HANDLE* handle) {
	auto res = syscall(srv::create_thread,func,arg,stk_size);
	return unpack_qword(res,handle);
}
void sleep(qword us) {
	syscall(srv::sleep,us);
}
dword check(HANDLE handle) {
	return syscall(srv::check,handle);
}
dword wait_for(HANDLE handle,qword us) {
	return syscall(srv::wait_for,handle,us);
}
dword signal(HANDLE handle,dword mode) {
	return syscall(srv::signal,handle,mode);
}
HANDLE get_process(void) {
	return syscall(srv::get_process);
}
dword process_id(HANDLE handle) {
	return syscall(srv::process_id,handle);
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
OBJTYPE handle_type(HANDLE handle) {
	return (OBJTYPE)syscall(srv::handle_type,handle);
}
STATUS open_handle(const char* name,dword length,HANDLE* handle){
	auto res = syscall(srv::open_handle,name,length);
	return unpack_qword(res,handle);
}
STATUS close_handle(HANDLE handle) {
	return (STATUS)syscall(srv::close_handle,handle);
}
STATUS create_object(OBJTYPE type,qword a1,qword a2,HANDLE* handle){
	auto res = syscall(srv::create_object,type,a1,a2);
	return unpack_qword(res,handle);
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
dword iostate(HANDLE handle){
	return syscall(srv::iostate,handle);
}
dword read(HANDLE handle,void* buffer,dword* length){
	auto res = syscall(srv::read,handle,buffer,*length);
	return unpack_qword<dword,dword>(res,length);
}
dword write(HANDLE handle,const void* buffer,dword* length){
	auto res = syscall(srv::write,handle,buffer,*length);
	return unpack_qword<dword,dword>(res,length);
}