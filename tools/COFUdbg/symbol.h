#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <windows.h>
#include "dbghelp.h"

class Symbol{
    HANDLE id;
    qword base;
    IMAGEHLP_LINE64 line_info;

    std::unordered_map<std::string,std::vector<std::string> > source;

    bool get_source(std::string&);

    static BOOL on_enum_symbol(SYMBOL_INFO*,ULONG,void*);

public:
    Symbol(const char*);
    ~Symbol(void);
    void match(const std::string&,std::map<qword,std::string>&);

    qword get(qword,std::string&);
    qword line_base(qword);
    unsigned line(qword,std::string&);
    qword next(qword);

};