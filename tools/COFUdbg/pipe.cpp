#include "pipe.h"
#include <stdexcept>
using namespace std;

Pipe::Pipe(const char* name) : hPipe(CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)){}

Pipe::~Pipe(void){
    if (hPipe != INVALID_HANDLE_VALUE)
        CloseHandle(hPipe);
}

byte Pipe::get(void){
    DWORD cnt = 0;
    byte buf = 0;
    if (!ReadFile(hPipe,&buf,1,&cnt,NULL) || cnt != 1){
        throw runtime_error("Pipe broken");
    }
    return buf;
}

void Pipe::read(string& str){
    while(true){
        while(get() != '$');
        byte chk_req = get();
        byte chk = get();
        byte cur = get();
        word len = chk | (cur << 8);
        chk += cur + '$';

        str.clear();
        while(len--){
            cur = get();
            chk += cur;
            str.append(1,cur);
        }

        if (chk == chk_req)
            return;
    }
}

void Pipe::write(const string& str){
    string buffer("$\0\0\0",4);
    byte chk = '$';

    for (byte data : str){
        buffer.append(1,data);
        chk += data;
    }
	chk += (buffer.at(2) = (byte)str.size());
	chk += (buffer.at(3) = (byte)(str.size() >> 8));
    buffer.at(1) = chk;

    DWORD len;
    if (!WriteFile(hPipe,buffer.data(),buffer.size(),&len,NULL) || len != buffer.size()){
        throw runtime_error("Pipe broken");
    }
}