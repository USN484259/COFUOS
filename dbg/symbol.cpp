#include "symbol.h"
#include <stdexcept>
using namespace std;


const HANDLE Symbol::id = (HANDLE)0xFF;

Symbol::Symbol(const char* name) {


	if (!SymInitialize(id, NULL, FALSE))
		throw runtime_error("SymInitialize");
	SymSetOptions(SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_LINES);
	base = SymLoadModuleEx(id, NULL, name, NULL, 0, 0, NULL, 0);
	if (!base)
		throw runtime_error("SymLoadModuleEx");

}

Symbol::~Symbol(void) {


	SymCleanup(id);
}

bool Symbol::resolve(qword addr, string& str) {
	byte buf[0x200];
	PSYMBOL_INFO info = (PSYMBOL_INFO)buf;
	info->SizeOfStruct = sizeof(SYMBOL_INFO);
	info->MaxNameLen = 0x200 - sizeof(SYMBOL_INFO);
	info->NameLen = 0;
	qword off;
	if (!SymFromAddr(id, addr, &off, info))
		return false;

	str.assign(info->Name, info->NameLen);
	return true;
}

qword Symbol::resolve(const string& name) {
	SYMBOL_INFO info;
	info.SizeOfStruct = sizeof(SYMBOL_INFO);
	info.MaxNameLen = 0;
	info.NameLen = 0;

	if (!SymFromName(id, name.c_str(), &info))
		return 0;

	return info.Address;
	
}

pair<const char*, dword> Symbol::source(qword addr) {
	IMAGEHLP_LINE64 line;
	line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
	DWORD off;
	if (!SymGetLineFromAddr64(id, addr, &off, &line))
		return pair<const char*,dword>(nullptr, 0);
	return pair<const char*, dword>(line.FileName, line.LineNumber);
}