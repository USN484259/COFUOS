#pragma once
#include "types.h"


typedef dword HANDLE;
enum ERROR_CODE : dword{
	DE = 0xC0000000,
	DB = 0xC0000001,
	BP = 0xC0000003,
	OF = 0xC0000004,
	BR = 0xC0000005,
	UD = 0xC0000006,
	SO = 0xC0000009,	//stack overflow
	SV = 0xC000000A,	//invalid service command
	NP = 0xC000000B,
	SS = 0xC000000C,
	GP = 0xC000000D,
	PF = 0xC000000E,
	MF = 0xC0000010,
	AC = 0xC0000011,
	XF = 0xC0000013
};
enum PRIVILEGE : byte {KERNEL = 0,SHELL = 0x20,NORMAL = 0x40};
enum REASON : byte {NONE = 0, PASSED = 1, NOTIFY = 2, TIMEOUT = 3, ABANDON = 4};
enum OBJTYPE : dword {UNKNOWN = 0,THREAD,PROCESS,STREAM,FILE,PIPE,SEMAPHORE,EVENT};
enum IOSTATE : byte {
	EOF = 1,
	FAIL = 2,
	BAD = 4
};
enum FILE_ATTRIBUTE : byte {
	READONLY = 1,
	HIDDEN = 2,
	SYSTEM = 4,
	//VID = 8,
	FOLDER = 0x10,
	ARCHIVE = 0x20
};
enum OPEN_MODE : byte {
	SHARE_READ = 1,
	SHARE_WRITE = 2
};
enum STATUS : dword {
	SUCCESS			= 0,
	FAILED			= 0x80000000,
	BAD_PARAM		= 0x80000001,
	BAD_BUFFER		= 0x80000002,
	TOO_SMALL		= 0x80000003,
	NOT_AVAILABLE	= 0x80000004,
	DENIED			= 0x80000005,
	NO_RESOURCE		= 0x80000006,
	BAD_HANDLE		= 0x80000007,
	NOT_FOUND		= 0x80000008,
};
struct OS_INFO{
	dword version;
	word features;
	word active_core;
	dword cpu_load;
	dword process_count;
	qword running_time;
	qword total_memory;
	qword used_memory;
	word resolution_x;
	word resolution_y;
	dword reserved;
	//system description follows
};
struct PROCESS_INFO{
	dword id;
	PRIVILEGE privilege;
	byte state;
	dword thread_count;
	dword handle_count;
	qword start_time;
	qword cpu_time;
	qword memory_usage;
};
struct STARTUP_INFO{
	char* commandline;
	char* environment;
	dword cmd_length;
	dword env_length;
	qword reserved;
	dword flags;
	HANDLE std_handle[3];
};
struct rectangle{
	word left;
	word top;
	word right;
	word bottom;
};
enum osctl_code : dword {
	bugcheck = 0,
	halt,
	disk_read
};