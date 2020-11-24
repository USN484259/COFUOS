#pragma once

#include "types.hpp"
#include "libudis86/extern.h"
#include <string>


class Disasm{
    ud_t ud;
    const byte* code;
    qword base;
    size_t length;
    qword next;
    void load_image(const char*);
public:
    Disasm(const char*);
    ~Disasm(void);

    unsigned get(qword,std::string&);

};