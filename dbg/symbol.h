#pragma once
#include "..\..\types.hpp"
#include <string>
#include "windows.h"
#include "dbghelp.h"
#pragma comment(lib,"Dbghelp.lib")

class Symbol {
	static const HANDLE id;
	DWORD64 base;
public:



	Symbol(const char*);
	~Symbol(void);

	bool resolve(qword,std::string&);
	qword resolve(const std::string&);
	std::pair<const char*, dword> source(qword);
};