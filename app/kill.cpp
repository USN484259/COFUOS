#include "uos.h"

int main(int argc,char** argv){
	if (argc < 2 || 0 == strcmp(argv[1],"--help")){
		printf("%s <pid>\nKill the process\n",argv[0]);
		return argc < 2;
	}
	dword pid = strtoul(argv[1],nullptr,0);
	HANDLE hps = 0;
	switch(open_process(pid,&hps)){
		case SUCCESS:
			break;
		case DENIED:
			puts("Access denied");
			return 5;
		default:
			puts("Failed");
			return 3;
	}
	return kill_process(hps,KL);
}