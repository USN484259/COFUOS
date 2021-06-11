#include "uos.h"

int main(int argc,char** argv){
	if (argc == 2 && 0 == strcmp(argv[1],"--help")){
		printf("%s [file_list]\nConcatenate files' content\n",argv[0]);
		return 0;
	}
	for (auto i = 1;i < argc;++i){
		HANDLE f = 0;
		dword stat;
		switch(file_open(argv[i],strlen(argv[i]),ALLOW_FILE,&f)){
			case SUCCESS:
				break;
			case DENIED:
				fprintf(stderr,"Access denied %s\n",argv[i]);
				return 5;
			case NOT_FOUND:
				fprintf(stderr,"No such file %s\n",argv[i]);
				return 3;
			default:
				fprintf(stderr,"Failed opening %s\n",argv[i]);
				return 1;
		}
		char buffer[0x201];
		do{
			stat = 0;
			dword sz = sizeof(buffer) - 1;
			if (SUCCESS != stream_read(f,buffer,&sz)){
				fprintf(stderr,"Failed reading file %s\n",argv[1]);
				return 5;
			}
			if (sz == 0){
				wait_for(f,0,0);
				stat = stream_state(f,&sz);
			}
			if (sz == 0){
				break;
			}
			buffer[sz] = 0;
			fputs(buffer,stdout);
		}while(0 == (stat & EOF_FAILURE));
		close_handle(f);
	}
	return 0;
}