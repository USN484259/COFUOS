#include "uos.h"

int main(int argc,char** argv){
	OS_INFO info[2];
	dword len = sizeof(info);
	if (SUCCESS != os_info(info,&len))
		return -1;

	printf("%s\nversion %x\nactive core %hu\nfeatures 0x%hx\nresolution %d*%d\n",\
		info + 1, info->version, info->active_core, \
		info->features, info->resolution_x, info->resolution_y
	);

	char total_unit = (info->total_memory > 0x100000) ? 'M' : 'K';
	unsigned total_divider = (total_unit == 'M') ? 0x100000 : 0x400;
	char used_unit = (info->used_memory > 0x100000) ? 'M' : 'K';
	unsigned used_divider = (used_unit == 'M') ? 0x100000 : 0x400;
	auto time = info->running_time / 1000;
	word ms = time % 1000;
	time /= 1000;
	byte sec = time % 60;
	time /= 60;
	byte min = time % 60;
	time /= 60;
	dword hour = time;

	printf("%u tasks\nmemory %llu%c/%llu%c\nup_time %d:%hhd:%hhd.%hd\n",\
		info->process_count, info->used_memory/used_divider, used_unit,\
		info->total_memory/total_divider, total_unit, \
		hour,min,sec,ms
	);
	return 0;
}