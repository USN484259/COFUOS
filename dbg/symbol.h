#pragma once
#include <string>
#include "windows.h"
#include "dbghelp.h"
#pragma comment(lib,"Dbghelp.lib")

#include "..\..\types.hpp"

class Symbol {
	static const HANDLE id;
	qword base;
public:

	Symbol(const char*);
	~Symbol(void);

	bool resolve(qword,std::string&);
	qword resolve(const std::string&);
	std::pair<const char*, dword> source(qword);
};