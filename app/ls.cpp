#include "uos.h"
#include "util.hpp"

int main(int argc,char** argv){
	char* buffer = nullptr;
	dword sz;
	if (argc >= 2){
		if (0 == strcmp(argv[1],"--help")){
			printf("%s [path]\nList content of path or working directory\n",argv[0]);
			return 0;
		}
		buffer = argv[1];
		sz = strlen(buffer);
	}
	else{
		sz = 0;
		if (TOO_SMALL != get_work_dir(buffer,&sz))
			return 5;
		sz = UOS::align_up(sz + 1,0x10);
		buffer = (char*)operator new(sz);
		--sz;
		if (SUCCESS != get_work_dir(buffer,&sz))
			return 5;
		buffer[sz] = 0;
	}
	HANDLE folder = 0;
	switch(file_open(buffer,sz,ALLOW_FOLDER,&folder)){
		case SUCCESS:
			break;
		case DENIED:
			fprintf(stderr,"Access denied %s\n",sz);
			return 5;
		case NOT_FOUND:
			fprintf(stderr,"No such directory %s\n",sz);
			return 3;
		default:
			fprintf(stderr,"Failed opening %s\n",sz);
			return 1;
	}
	char name[0x100];
	do{
		sz = sizeof(name) - 1;
		dword stat = 0;
		if (SUCCESS != stream_read(folder,name,&sz)){
			fputs("Failed reading folder",stderr);
			return 5;
		}
		if (sz == 0){
			wait_for(folder,0,0);
			stat = stream_state(folder,&sz);
		}
		if (sz){
			name[sz] = 0;
			puts(name);
		}
		if (stat & EOF_FAILURE)
			break;
	}while(true);
	return 0;
}