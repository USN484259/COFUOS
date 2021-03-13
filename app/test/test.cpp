#include "uos.h"


int main(int argc,char** argv){
	for (auto i = 0;i < argc;++i){
		auto len = strlen(argv[i]);
		dbg_print(argv[i],len);
	}
	return 0;
}
