#include "uos.h"
#include "util.hpp"

int main(int argc,char** argv){
	auto tail = argv[0] + strlen(argv[0]);
	auto self_name = UOS::find_last_of(argv[0],tail,'/');
	if (self_name == tail)
		self_name = argv[0];
	else
		++self_name;

	char name[0x100];
	dword sz = sizeof(name) - 1;
	HANDLE h = get_process();
	if (SUCCESS != get_command(h,name,&sz)){
		fputs("Failed getting commandline\n",stderr);
		return 1;
	}
	tail = name + sz;
	auto pos = UOS::find_last_of(name,tail,'/');
	++pos;

	if (SUCCESS != file_open(name,pos - name,ALLOW_FOLDER,&h)){
		*pos = 0;
		fprintf(stderr,"Failed opening folder %s\n",name);
		return 1;
	}
	do{
		dword stat = 0;
		sz = sizeof(name) - (pos - name) - 1;
		if (SUCCESS != stream_read(h,pos,&sz)){
			fputs("Failed reading folder\n",stderr);
			return 1;
		}
		if (sz == 0){
			wait_for(h,0,0);
			stat = stream_state(h,&sz);
		}
		if (sz){
			sz += (pos - name);
			name[sz] = 0;
			if (0 != strcmp(pos,self_name)){
				STARTUP_INFO info = {0};
				info.commandline = name;
				info.cmd_length = sz;
				info.std_handle[0] = 1;
				info.std_handle[1] = 2;
				info.std_handle[2] = 3;
				HANDLE ps = 0;
				dword result = create_process(&info,sizeof(info),&ps);
				if (SUCCESS != result){
					printf("%s launch failed 0x%x\n",name,result);
				}
				else{
					switch(wait_for(ps,4*1000*1000,0)){
						case PASSED:
						case NOTIFY:
							if (SUCCESS != process_result(ps,&result)){
								fprintf(stderr,"Failed getting result of %s\n",name);
							}
							else{
								printf("%s exited with 0x%x\n",name,result);
							}
							break;
						case TIMEOUT:
							printf("%s timeout, ",name);
							if (SUCCESS == kill_process(ps,0x80000000)){
								puts("killed");
							}
							else{
								fprintf(stderr,"Failed to kill\n");
							}
							break;
						default:
							fprintf(stderr,"Unknown state of %s\n",name);
					}
					close_handle(ps);
				}
			}
		}
		if (stat & EOF_FAILURE)
			break;
		
	}while(true);
	return 0;
}