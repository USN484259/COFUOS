#include "uos.h"

static const char* hexchar = "0123456789ABCDEF";

int main(int argc,char** argv){
	if (argc < 2){
		printf("%s (LBA)\n",argv[0]);
		return 1;
	}

	qword lba = strtoull(argv[1],nullptr,0);
	
	byte buffer[0x200];
	dword size = sizeof(buffer);
	*(qword*)buffer = lba;

	switch(osctl(disk_read,buffer,&size)){
		case DENIED:
			printf("access denied\n");
			return 5;
		case SUCCESS:
			break;
		default:
			printf("failed reading LBA 0x%llx\n",lba);
			return 2;

	}
	for (auto page = 0;page < 2;++page){
		if (page){
			printf("--more--");
			auto res = wait_for(stdin,0);
			assert(res == NOTIFY || res == PASSED);
			do{
				byte buffer[0x10];
				dword size = sizeof(buffer);
				res = read(stdin,buffer,&size);
				assert(res == 0);
				if (size == 0)
					break;
			}while(true);
		}
		printf("dump of LBA 0x%llx :\n",lba);
		//printf("\t00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
		for (auto line = page*0x10;line < (page + 1)*0x10;++line){
			char str[0x40] = "  0\t";
			str[0] = hexchar[line >> 4];
			str[1] = hexchar[line & 0x0F];
			for (auto i = 0;i < 0x10;++i){
				byte data = buffer[line*0x10 + i];
				str[4 + i*3] = hexchar[data >> 4];
				str[5 + i*3] = hexchar[data & 0x0F];
				str[6 + i*3] = ' ';
			}
			str[3 + 0x30] = '\n';
			str[4 + 0x30] = 0;
			fputs(str,stdout);
		}
	}
	return 0;
}