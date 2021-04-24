#pragma once
#include "types.h"
#include "kernel/interface/include/interface.h"

#ifdef __cplusplus
extern "C" {
#endif

enum STATUS		osctl(enum osctl_code cmd,void* buffer,dword* length);
enum STATUS		os_info(struct OS_INFO* buffer,dword* length);
qword			get_time(void);
enum STATUS		enum_process(dword* id);
enum STATUS		display_fill(dword color,const struct rectangle* rect);
enum STATUS		display_draw(const dword* buffer,const struct rectangle* rect,word advance);
HANDLE			get_thread(void);
dword			thread_id(HANDLE handle);
void*			get_handler(void);
dword			get_priority(HANDLE handle);
void			exit_thread(void);
enum STATUS		kill_thread(HANDLE handle);
enum STATUS		set_handler(void *func);
enum STATUS		set_priority(HANDLE handle,byte priority);
enum STATUS		create_thread(void (*func)(void*,void*),void* arg,dword stk_size,HANDLE* handle);
void			sleep(qword us);
dword			check(HANDLE handle);
dword			wait_for(HANDLE handle,qword us);
dword			signal(HANDLE handle,dword mode);
HANDLE			get_process(void);
dword			process_id(HANDLE handle);
enum STATUS		process_info(HANDLE handle,struct PROCESS_INFO* buffer,dword* length);
enum STATUS		get_command(HANDLE handle,char* buffer,dword* length);
void			exit_process(dword result);
enum STATUS		kill_process(HANDLE handle,dword result);
enum STATUS		process_result(HANDLE handle,dword* result);
enum STATUS		create_process(const struct STARTUP_INFO* info,dword length,HANDLE* handle);
enum STATUS		open_process(dword id,HANDLE* handle);
enum OBJTYPE	handle_type(HANDLE handle);
enum STATUS		open_handle(const char* name,dword length,HANDLE* handle);
enum STATUS		close_handle(HANDLE handle);
enum STATUS		create_object(OBJTYPE type,qword a1,qword a2,HANDLE* handle);
qword			vm_peek(void* va);
enum STATUS		vm_protect(void* base,dword count,qword attrib);
void*			vm_reserve(void* base,dword count);
enum STATUS		vm_commit(void* base,dword count);
enum STATUS		vm_release(void* base,dword count);
dword			iostate(HANDLE handle,dword* count);
dword			read(HANDLE handle,void* buffer,dword* length);
dword			write(HANDLE handle,const void* buffer,dword* length);

#ifdef __cplusplus
}
#endif