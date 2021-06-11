#pragma once
#include "types.h"
#include "kernel/interface/include/interface.h"

#ifdef __cplusplus
extern "C" {
#endif

STATUS		osctl(osctl_code cmd,void* buffer,dword* length);
STATUS		os_info(OS_INFO* buffer,dword* length);
qword		get_time(void);
STATUS		enum_process(dword* id);
STATUS		display_fill(dword color,const rectangle* rect);
STATUS		display_draw(const dword* buffer,const rectangle* rect,word advance);
HANDLE		get_thread(void);
dword		thread_id(HANDLE handle);
void*		get_handler(void);
dword		get_priority(HANDLE handle);
void		exit_thread(void);
STATUS		kill_thread(HANDLE handle);
STATUS		set_handler(void *func);
STATUS		set_priority(HANDLE handle,byte priority);
STATUS		create_thread(void (*func)(void*,void*),void* arg,dword stk_size,HANDLE* handle);
void		sleep(qword us);
dword		wait_for(HANDLE handle,qword us,dword nowait);
dword		signal(HANDLE handle,dword mode);
HANDLE		get_process(void);
dword		process_id(HANDLE handle);
STATUS		process_info(HANDLE handle,PROCESS_INFO* buffer,dword* length);
STATUS		get_command(HANDLE handle,char* buffer,dword* length);
void		exit_process(dword result);
STATUS		kill_process(HANDLE handle,dword result);
STATUS		process_result(HANDLE handle,dword* result);
STATUS		create_process(const STARTUP_INFO* info,dword length,HANDLE* handle);
STATUS		open_process(dword id,HANDLE* handle);
STATUS		get_work_dir(char* buffer,dword* length);
STATUS		set_work_dir(const char* path,dword length);
OBJTYPE		handle_type(HANDLE handle);
STATUS		open_handle(const char* name,dword length,HANDLE* handle);
STATUS		close_handle(HANDLE handle);
STATUS		create_object(OBJTYPE type,qword a1,qword a2,HANDLE* handle);
qword		vm_peek(void* va);
STATUS		vm_protect(void* base,dword count,qword attrib);
void*		vm_reserve(void* base,dword count);
STATUS		vm_commit(void* base,dword count);
STATUS		vm_release(void* base,dword count);
dword		stream_state(HANDLE handle,dword* count);
dword		stream_read(HANDLE handle,void* buffer,dword* length);
dword		stream_write(HANDLE handle,const void* buffer,dword* length);
STATUS		file_open(const char* name,dword length,dword mode,HANDLE* handle);
STATUS		file_tell(HANDLE handle,qword* buffer);
STATUS		file_seek(HANDLE handle,qword offset,dword mode);
STATUS		file_setsize(HANDLE handle,qword new_size);
STATUS		file_path(HANDLE handle,char* buffer,dword* length);
STATUS		file_info(HANDLE handle,const FILE_INFO* buffer,dword* length);
STATUS		file_change(HANDLE handle,dword attrib);
STATUS		file_move(HANDLE handle,const char* target,dword length);

#ifdef __cplusplus
}
#endif