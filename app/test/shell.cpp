#include "uos.h"

int main(int argc,char** argv){
	char str[0x40];
	dword len = 1 + strlen(argv[0]);
	write(stderr,argv[0],&len);
	while(true){
		byte buffer[0x10];
		dword size = 0x10;
		if (0 != read(stdin,buffer,&size))
			break;
		if (size == 0){
			wait_for(stdin,0);
			continue;
		}
		for (unsigned i = 0;i < size;++i){
			auto key = buffer[i] & 0x7F;
			bool state = buffer[i] & 0x80;
			len = 1 + snprintf(str,sizeof(str),"key %hhx (%c) %sd",key,isprint(key) ? key : '?',state ? "release" : "press");
			write(stderr,str,&len);
		}
	}
	return 0;
}