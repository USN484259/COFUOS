#include "symbol.h"
#include <cmath>
#include <fstream>

using namespace std;

#pragma comment(lib,"dbghelp.lib")

Symbol::Symbol(const char* name) : id((HANDLE)(rand() + 1)), base(0), line_info{sizeof(IMAGEHLP_LINE64)}{
    do{
        if (!SymInitialize(id, NULL, FALSE))
            break;
        SymSetOptions(SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_LINES);
        base = SymLoadModuleEx(id, NULL, name, NULL, 0, 0, NULL, 0);
        if (!base)
            break;
        
        return;
    }while(false);
    throw runtime_error(string("Symbol load failed : ").append(name));
}

Symbol::~Symbol(void){
    if (id)
        SymCleanup(id);
}



void Symbol::match(const string& pattern,map<qword,string>& table){
    SymEnumSymbols(id,base,pattern.c_str(),on_enum_symbol,static_cast<void*>(&table));
}

BOOL Symbol::on_enum_symbol(SYMBOL_INFO* info,ULONG,void* p){
    auto& table = *static_cast<map<qword,string>*>(p);
    if (info->Address && info->NameLen)
        table.emplace((qword)info->Address,string(info->Name));
    return TRUE;
}

qword Symbol::get(qword addr,string& name){
    byte buffer[0x400] = {0};
    SYMBOL_INFO* info = (SYMBOL_INFO*)buffer;
    info->SizeOfStruct = sizeof(SYMBOL_INFO);
    info->MaxNameLen = sizeof(buffer) - sizeof(SYMBOL_INFO);

    qword displacement;
    if (!SymFromAddr(id,addr,&displacement,info))
        return 0;
    name = info->Name;
    return info->Address;
}

qword Symbol::line_base(qword addr){
    //if (addr != line_info.Address){
        DWORD displacement;
        if (!SymGetLineFromAddr64(id,addr,&displacement,&line_info))
            return 0;
    //}
    return line_info.Address;
}

bool Symbol::get_source(string& str){
    // line_info.FileName could be :
    // D:\COFUOS\tools\COFUdbg\d:\cofuos\kernel\memory\pm.cpp
    string filename(line_info.FileName);

    auto pos = filename.find_last_of(':');
    if (pos && pos != string::npos){
        filename = filename.substr(pos - 1);
    }

    auto it = source.find(filename);
    if (it == source.cend()){
        vector<string> lines;
        ifstream file(filename);
        while(file.good()){
            string line;
            getline(file,line);
            lines.push_back(move(line));
        }
        if (lines.empty())
            return false;
        it = source.emplace(move(filename),move(lines)).first;
    }
    const auto& lines = it->second;
    auto line = line_info.LineNumber;
    if (!line || line > lines.size())
        return false;
    str.assign(lines.at(line - 1));
    return true;
}

unsigned Symbol::line(qword addr,string& str){
    //if (addr != line_info.Address){
        DWORD displacement;
        if (!SymGetLineFromAddr64(id,addr,&displacement,&line_info))
            return 0;
    //}
    if (get_source(str))
        return line_info.LineNumber;

    return 0;
}

qword Symbol::next(qword addr){
    //if (addr != line_info.Address){
        DWORD displacement;
        if (!SymGetLineFromAddr64(id,addr,&displacement,&line_info))
            return 0;   
    //}
    if (!SymGetLineNext64(id,&line_info))
        return 0;
    return line_info.Address;
}