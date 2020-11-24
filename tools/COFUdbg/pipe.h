#pragma once
#include "types.hpp"
#include <windows.h>
#include <string>

class Pipe{
    HANDLE hPipe;
    byte get(void);
public:
    Pipe(const char*);
    ~Pipe(void);

    void read(std::string&);
    void write(const std::string&);
    //void write(const char*);
};
