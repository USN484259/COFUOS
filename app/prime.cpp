#include "uos.h"
#include "math.h"
#include "vector.hpp"

using namespace UOS;

int main(int argc,char** argv){
	qword limit = 0;
	if (argc > 1){
		if (0 == strcmp(argv[1],"--help")){
			printf("%s [limit]\tCalculate prime numbers up to given limit\n",argv[0]);
			return 1;
		}
		limit = strtoull(argv[1],nullptr,0);
	}
	if (limit < 3)
		limit = 100;
	vector<qword> table;
	table.push_back(2);
	table.push_back(3);

	for (qword num = 4;num <= limit;++num){
		auto top = (qword)sqrt(num);
		bool found = false;
		for (auto& divider : table){
			if (divider > top){
				found = true;
				break;
			}
			if (0 == (num % divider))
				break;
		}
		if (found)
			table.push_back(num);
	}
	for (auto& val : table){
		printf("%llu\n",val);
	}
	return 0;
}