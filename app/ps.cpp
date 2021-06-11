#include "uos.h"
#include "util.hpp"

using namespace UOS;

static constexpr dword cmd_size = 0x200;

int list_process(void){
	dword pid = 0;
	do{
		if (SUCCESS != enum_process(&pid)){
			fputs("failed to enumerate process\n",stderr);
			return 1;
		}
		HANDLE hps = 0;
		if (SUCCESS != open_process(pid,&hps)){
			printf("%d\t?\n",pid);
			continue;
		}

		char cmd[cmd_size];

		dword size = sizeof(cmd);
		if (SUCCESS != get_command(hps,cmd,&size)){
			printf("%d\t<too long>\n",pid);
		}
		else{
			cmd[min<dword>(size,sizeof(cmd) - 1)] = 0;
			printf("%d\t%s\n",pid,cmd);
		}
		close_handle(hps);
	}while(pid);
	return 0;
}

int process_detail(dword pid){
	HANDLE hps = 0;
	switch(open_process(pid,&hps)){
		case SUCCESS:
			break;
		case DENIED:
			fputs("Access denied\n",stderr);
			return 5;
		default:
			fputs("Failed\n",stderr);
			return 3;
	}
	int result = 0;
	do{
		PROCESS_INFO info;
		dword size = sizeof(info);
		if (SUCCESS != process_info(hps,&info,&size) || size != sizeof(info)){
			fprintf(stderr,"failed to get detail of process %u\n",pid);
			result = 3;
			break;
		}
		char cmd[cmd_size];
		size = sizeof(cmd);
		if (SUCCESS != get_command(hps,cmd,&size))
			size = 0;
		cmd[min<dword>(size,sizeof(cmd) - 1)] = 0;

		printf("process-id\t%d\n",pid);
		printf("commandline\t%s\n",size ? cmd : "<too long>");
		printf("privilege\t%s\n",info.privilege <= SHELL ? "SHELL" : "NORMAL");
		{
			char mem_unit = 'K';
			dword mem_val = info.memory_usage / 0x400;
			if (mem_val >= 0x400){
				mem_unit = 'M';
				mem_val /= 0x400;
			}
			printf("memory\t%d%cB\n",mem_val,mem_unit);
		}
		printf("thread-count\t%d\n",info.thread_count);
		printf("handle-count\t%d\n",info.handle_count);
		{
			dword ms = info.start_time / 1000;
			dword sec = ms / 1000;
			ms %= 1000;
			printf("start-time\t%d:%d\n",sec,ms);
		}
		{
			dword ms = info.cpu_time / 1000;
			dword sec = ms / 1000;
			ms %= 1000;
			printf("cpu-time\t%d:%d\n",sec,ms);
		}
	}while(false);
	close_handle(hps);
	return result;
}


int main(int argc,char** argv){
	if (argc < 2)
		return list_process();
	if (0 == strcmp(argv[1],"--help")){
		printf("%s [pid]\nShow process detail. List processes if no pid specified\n",argv[0]);
		return 0;
	}
	dword pid = strtoul(argv[1],nullptr,0);
	return process_detail(pid);
}