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

    decltype(source)::const_iterator it_file;

    bool get_source(void);

    static BOOL on_enum_symbol(SYMBOL_INFO*,ULONG,void*);

public:
    Symbol(const char*);
    ~Symbol(void);
    void match(const std::string&,std::map<qword,std::string>&);

    qword get_symbol(qword,std::string&);
    qword get_line(qword);
    qword next_line(void);
    unsigned line_number(void) const;
    const std::string& line_file(void) const;
    const std::string& line_content(void) const;
};