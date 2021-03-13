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
enum PRIVILEGE : byte {NORMAL = 0,DEBUGGER = 0x20,SHELL = 0x40,KERNEL = 0x7F};
enum REASON : byte {NONE = 0, PASSED = 1, NOTIFY = 2, TIMEOUT = 3};
enum OBJTYPE : dword {UNKNOWN = 0,THREAD,PROCESS,FILE,MUTEX,EVENT};
enum STATUS : dword {
	SUCCESS = 0,
	PENDING = 1,
	FAILED = 0x80000000,
	BAD_PARAM = 0x80000001,
	BAD_BUFFER = 0x80000002,
	TOO_SMALL = 0x80000003,
	DENIED = 0x80000005,
	NO_RESOURCE = 0x80000006,
	BAD_HANDLE = 0x80000007
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
	qword reserved;
	//system description follows
};
struct PROCESS_INFO{
	dword id;
	PRIVILEGE privilege;
	byte state;
	dword thread_count;
	dword handle_count;
	qword start_time;
	qword cpu_time_ms;
};
struct STARTUP_INFO{
//#error TODO
};
struct rectangle{
	dword left;
	dword top;
	dword right;
	dword bottom;
};
