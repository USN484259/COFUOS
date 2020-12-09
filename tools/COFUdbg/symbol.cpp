#include "symbol.h"
#include <fstream>

using namespace std;

#pragma comment(lib,"dbghelp.lib")

Symbol::Symbol(const char* name) : id(GetCurrentProcess()), base(0), line_info{sizeof(IMAGEHLP_LINE64)}{
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
    //weird crash on wine 5.0.3
    //if (id)
    //    SymCleanup(id);
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

qword Symbol::get_symbol(qword addr,string& name){
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

qword Symbol::get_line(qword addr){
    DWORD displacement;
    if (!SymGetLineFromAddr64(id,addr,&displacement,&line_info))
        return 0;
    if (0 == line_info.LineNumber)
        return 0;
    if (!get_source())
        return 0;
    return line_info.Address;  
}

qword Symbol::next_line(void){
    if (!SymGetLineNext64(id,&line_info))
        return 0;
    if (0 == line_info.LineNumber)
        return 0;
    if (!get_source())
        return 0;
    return line_info.Address;
}

unsigned Symbol::line_number(void) const{
    return line_info.LineNumber;
}

const string& Symbol::line_file(void) const{
    if (it_file == source.cend())
        throw runtime_error("Invalid source file");
    return it_file->first;
}

const string& Symbol::line_content(void) const{
    if (it_file == source.cend())
        throw runtime_error("Invalid source file");
    auto line = line_number();
    const auto& lines = it_file->second;
    if (line && line <= lines.size())
        return lines.at(line - 1);
    throw runtime_error("Invalid source line");
}

bool Symbol::get_source(void){
    // line_info.FileName could be :
    // D:\COFUOS\tools\COFUdbg\d:\cofuos\kernel\memory\pm.cpp
    string filename(line_info.FileName);
    {
        auto pos = filename.find_last_of(':');
        if (pos && pos != string::npos){
            filename = filename.substr(pos - 1);
        }
    }
    it_file = source.find(filename);
    if (it_file == source.cend()){
        vector<string> lines;
        ifstream file(filename);
        while(file.good()){
            string line;
            getline(file,line);
            auto pos = line.find_first_not_of('\t');
            lines.push_back(move( pos == string::npos ? line : line.substr(pos) ));
        }
        if (lines.empty())
            return false;
        it_file = source.emplace(move(filename),move(lines)).first;
    }
    return true;
}
