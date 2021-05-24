#pragma once
#include "types.h"


typedef dword HANDLE;
typedef enum : dword{
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
} ERROR_CODE;
typedef enum : byte {KERNEL = 0,SHELL = 0x20,NORMAL = 0x40} PRIVILEGE;
typedef enum : byte {NONE = 0, PASSED = 1, NOTIFY = 2, TIMEOUT = 3, ABANDON = 4} REASON;
typedef enum : dword {OBJ_UNKNOWN = 0,OBJ_THREAD,OBJ_PROCESS,OBJ_STREAM,OBJ_FILE,OBJ_PIPE,OBJ_SEMAPHORE,OBJ_EVENT} OBJTYPE;
typedef enum : byte {
	// EOF_BIT = 1,
	// FAIL_BIT = 2,
	// BAD_BIT = 4
	EOF_FAILURE = 1,
	OP_FAILURE = 4,
	MEM_FAILURE = 8,
	FS_FAILURE = 0x10,
	MEDIA_FAILURE = 0x20,
} IOSTATE;
typedef enum : byte {
	READONLY = 1,
	HIDDEN = 2,
	SYSTEM = 4,
	//VID = 8,
	FOLDER = 0x10,
	ARCHIVE = 0x20
} FILE_ATTRIBUTE;
typedef enum : byte {
	SHARE_READ = 1,
	SHARE_WRITE = 2
} OPEN_MODE;
typedef enum : dword {
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
} STATUS;
typedef struct {
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
} OS_INFO;
typedef struct {
	dword id;
	PRIVILEGE privilege;
	byte state;
	dword thread_count;
	dword handle_count;
	qword start_time;
	qword cpu_time;
	qword memory_usage;
} PROCESS_INFO;
typedef struct {
	char* commandline;
	char* work_dir;
	char* environment;
	dword cmd_length;
	dword wd_length;
	dword env_length;
	dword flags;
	HANDLE std_handle[3];
} STARTUP_INFO;
typedef struct {
	dword attribute;
} FILE_INFO;

typedef struct {
	word left;
	word top;
	word right;
	word bottom;
} rectangle;
typedef enum : dword {
	bugcheck = 0,
	halt,
	disk_read,
	dbgbreak = 3,
} osctl_code;