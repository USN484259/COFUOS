#include "uos.h"
#include "util.hpp"

int main(int argc,char** argv){
	dword sz = 0;
	if (TOO_SMALL != get_work_dir(nullptr,&sz))
		return 5;
	sz = UOS::align_up(sz + 1,0x10);
	char buffer[sz];
	if (SUCCESS != get_work_dir(buffer,&sz))
		return 5;
	buffer[sz] = 0;
	puts(buffer);
	return 0;
}