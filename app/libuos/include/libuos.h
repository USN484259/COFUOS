#pragma once
#include "types.h"
#include "kernel/interface/include/interface.h"

#ifdef __cplusplus
extern "C" {
#endif

enum STATUS		os_info(struct OS_INFO* buffer,dword* length);
qword			get_time(void);
enum STATUS		enum_process(dword* id);
enum STATUS		get_message(void* buffer,dword* length);
void			dbg_print(const char* str,dword length);
enum STATUS		display_fill(dword color,const struct rectangle* rect);
enum STATUS		display_draw(const dword* buffer,const struct rectangle* rect);
HANDLE			get_thread(void);
enum STATUS		thread_id(HANDLE handle,dword* id);
void*			get_handler(void);
enum STATUS		get_priority(HANDLE handle,byte* priority);
__attribute__((noreturn))
void			exit_thread(void);
enum STATUS		kill_thread(HANDLE handle);
enum STATUS		set_handler(void* func);
enum STATUS		set_priority(HANDLE handle,byte priority);
enum STATUS		create_thread(void* func,void* arg,dword stk_size,HANDLE* handle);
void			sleep(qword us);
enum STATUS		check(HANDLE handle,byte* state);
enum STATUS		wait_for(HANDLE handle,qword us,enum REASON* reason);
HANDLE			get_process(void);
enum STATUS		process_id(HANDLE handle,dword* id);
enum STATUS		process_info(HANDLE handle,struct PROCESS_INFO* buffer,dword* length);
enum STATUS		get_command(HANDLE handle,char* buffer,dword* length);
__attribute__((noreturn))
void			exit_process(dword result);
enum STATUS		kill_process(HANDLE handle,dword result);
enum STATUS		process_result(HANDLE handle,dword* result);
enum STATUS		create_process(const struct STARTUP_INFO* info,dword length,HANDLE* handle);
enum STATUS		open_process(dword id,HANDLE* handle);
enum STATUS		handle_type(HANDLE handle,enum OBJTYPE* type);
enum STATUS		close_handle(HANDLE handle);
qword			vm_peek(void* va);
enum STATUS		vm_protect(void* base,dword count,qword attrib);
void*			vm_reserve(void* base,dword count);
enum STATUS		vm_commit(void* base,dword count);
enum STATUS		vm_release(void* base,dword count);

#ifdef __cplusplus
}
#endif