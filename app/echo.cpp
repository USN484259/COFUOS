#include "uos.h"

int main(int argc,char** argv){
	for (auto i = 1;i < argc;++i){
		if (i != 1){
			putchar(' ');
		}
		fputs(argv[i],stdout);
	}
	putchar('\n');
	return 0;
}