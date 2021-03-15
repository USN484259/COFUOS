#include "uos.h"

inline qword read_rsp(void){
	qword val;
	__asm__ volatile (
		"mov %0, rsp"
		: "=rm" (val)
	);
	return val;
}


__attribute__((naked,noreturn))
void on_exception(void){
	__asm__ volatile (
		"mov ecx,[rsp + 8]\n\
		call next"
	);
}

extern "C"
void next(dword errcode){
	char buffer[0x40];
	auto size = snprintf(buffer,sizeof(buffer),"captured with %x @ %p",errcode,read_rsp());
	dbg_print(buffer,size);
	//sleep(0);
	set_handler((void*)on_exception);
	__asm__ volatile (
		"ud2"
	);
}

int main(int argc,char** argv){
	char buffer[0x200];
	sleep(0);
	if (argc == 1){
		OS_INFO info[2];
		dword size = 2*sizeof(OS_INFO);
		if (SUCCESS == os_info(info,&size)){
			char total_unit = (info->total_memory > 0x100000) ? 'M' : 'K';
			unsigned total_divider = (total_unit == 'M') ? 0x100000 : 0x400;
			char used_unit = (info->used_memory > 0x100000) ? 'M' : 'K';
			unsigned used_divider = (used_unit == 'M') ? 0x100000 : 0x400;
			size = snprintf(buffer,sizeof(buffer),\
				"%s version %x, %hu cores, %u processes, memory (%llu%c/%llu%c)",\
				info + 1, info->version, info->active_core, info->process_count,\
				info->used_memory/used_divider, used_unit,\
				info->total_memory/total_divider, total_unit
			);
			dbg_print(buffer,size);
		}
		kill_thread(get_thread());
	}
	//argc >= 2
	dword spin_count = 0;
	do{
		if (spin_count == 0){
			auto size = snprintf(buffer,sizeof(buffer),"%s %s",argv[0],argv[1]);
			dbg_print(buffer,size);
		}
		switch(~0x20 & *argv[1]){
			case 'L':	//sleep
				break;
			case 'P':	//spin
				spin_count = (spin_count + 1) & 0x0FFFFFFF;
				continue;
			case 'F':	//kill self
				kill_process(get_process(),0x55AA);

			case 'Y':	//try
				set_handler((void*)on_exception);
			case 'V':	//violation
				((PROCESS_INFO volatile*)0)->id = 0;
			default:	//kill (pid)
			{
				dword pid = strtoul(argv[1],nullptr,0);
				HANDLE ps;
				if (SUCCESS == open_process(pid,&ps)){
					dword size = snprintf(buffer,sizeof(buffer),"%s to kill process %d",\
						(SUCCESS == kill_process(ps,0xAA55)) ? "succeed" : "failed",pid);
					dbg_print(buffer,size);

					PROCESS_INFO info;
					size = sizeof(PROCESS_INFO);
					if (SUCCESS == process_info(ps,&info,&size)){
						char time_unit = (info.cpu_time > (1000*1000)) ? ' ' : 'm';
						unsigned time_divider = (time_unit == ' ') ? (1000*1000) : 1000;
						char mem_unit = (info.memory_usage > 0x100000) ? 'M' : 'K';
						unsigned mem_divider = (mem_unit == 'M') ? 0x100000 : 0x400;
						size = snprintf(buffer,sizeof(buffer),\
							"process %u, privilege %hhu, %u threads, %u handles, %s, start at %llu, cpu time %llu%cs, memory %llu%cB",\
							info.id, info.privilege, info.thread_count, info.handle_count, (info.state ? "running" : "stopped"),\
							info.start_time/(1000*1000), info.cpu_time/time_divider, time_unit,\
							info.memory_usage/mem_divider, mem_unit
						);
						dbg_print(buffer,size);
					}
					
				}
				else{
					auto size = snprintf(buffer,sizeof(buffer),"failed to open process %d",pid);
					dbg_print(buffer,size);
				}
				exit(0);
			}
		}
		sleep(1000*1000);
	}while(true);

	return 0;
}
