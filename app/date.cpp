#include "uos.h"

void show_date(qword time){
	//TODO
}
void show_time(qword time){
	byte sec = time % 60;
	time /= 60;
	byte min = time % 60;
	time /= 60;
	byte hour = time % 24;
	char str[10];
	str[0] = '0' + hour / 10;
	str[1] = '0' + hour % 10;
	str[2] = ':';
	str[3] = '0' + min / 10;
	str[4] = '0' + min % 10;
	str[5] = ':';
	str[6] = '0' + sec / 10;
	str[7] = '0' + sec % 10;
	str[8] = 0;
	fputs(str,stdout);
}

int main(int argc,char** argv){
	if (argc < 2){
		printf("%llu\n",get_time());
		return 0;
	}
	else if (0 == strcmp(argv[1],"--help")){
		printf("%s [options]\nShow Epoch time or date & time\n",argv[0]);
		puts("Options:");
		puts("-t\tShow time");
		puts("-d\tShow date");
		puts("-h\tShow date & time");
		return 0;
	}
	else do{
		if (argv[1][0] != '-')
			break;
		auto opt = argv[1][1];
		switch(opt){
			case 't':
			case 'd':
			case 'h':
				break;
			default:
				continue;
		}
		auto time = get_time();
		// adjust GMT
		constexpr int GMT = +8;
		time += GMT*3600;
		if (opt == 'd' || opt == 'h'){
			show_date(time);
		}
		if (opt == 'h')
			putchar('\t');
		if (opt == 't' || opt == 'h'){
			show_time(time);
		}
		putchar('\n');
		return 0;
	}while(false);
	puts("Unknown options");
	return 3;
}