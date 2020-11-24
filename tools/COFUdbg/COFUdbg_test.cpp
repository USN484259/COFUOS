#include <iostream>
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib,"dbghelp.lib")

using namespace std;

static_assert(sizeof(size_t) == 8,"size_t size mismatch");


BOOL on_enum_line(SRCCODEINFO* info,void*){
	cout << hex << info->Address << '\t' << info->FileName << '\t' << dec << info->LineNumber << endl;
	return TRUE;
}



int main(int argc,char** argv){
	const char* errmsg = nullptr;
	
	HANDLE id = (HANDLE)(rand() & 0xFF);
	
	size_t base = 0;
	
	do{
		if (argc < 2){
			errmsg = "File not specified";
			break;
		}
		if (!SymInitialize(id, NULL, FALSE)){
			errmsg = "SymInitialize Failed";
			break;
		}
		SymSetOptions(SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_LINES);
		base = SymLoadModuleEx(id, NULL, argv[1], NULL, 0, 0, NULL, 0);
		if (!base){
			errmsg = "SymLoadModuleEx Failed";
			break;
		}
		
		if (!SymEnumLines(id,base,NULL,NULL,on_enum_line,NULL)){
			errmsg = "SymEnumLines Failed";
			break;
		}
		
		while(true){
			size_t addr = 0;
			cin >> hex >> addr;
			if (0 == addr)
				break;
			IMAGEHLP_LINE64 info = {sizeof(IMAGEHLP_LINE64)};
			DWORD displacement;
			if (!SymGetLineFromAddr64(id,addr,&displacement,&info)){
				errmsg = "SymGetLineFromAddr64 Failed";
				break;
			}
			
			cout << hex << info.Address << '\t' << info.FileName << '\t' << dec << info.LineNumber << endl;
		
		}

	}while(false);
	if (errmsg){
		cerr << errmsg << endl;
		return 1;
	}
	return 0;
}
