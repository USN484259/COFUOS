#include "disasm.h"
#include <fstream>
using namespace std;

#pragma comment(lib,"libudis86/udis86.lib")

Disasm::Disasm(const char* pe_name) : code(nullptr),base(0),length(0),next(0){
    ud_init(&ud);
    ud_set_mode(&ud,64);
    ud_set_vendor(&ud,UD_VENDOR_INTEL);
    ud_set_syntax(&ud,UD_SYN_INTEL);

    load_image(pe_name);
}

Disasm::~Disasm(void){
    if (code)
        delete[] code;
}

unsigned Disasm::get(qword addr){
    if (addr < base)
        return 0;
    auto off = addr - base;
    if (off >= length)
        return 0;

    if (addr != next){
        ud_set_input_buffer(&ud,code + off,length - off);
        ud_set_pc(&ud,addr);
    }
    return ud_disassemble(&ud);
}

unsigned Disasm::get(qword addr,std::string& str){
    auto len = get(addr);
    if (len){
        str.assign(ud_insn_asm(&ud));
        next = addr + len;
    }
    return len;
}


struct PE_INFO{
    dword signature;
    word machine;
    word section_count;
    dword time_stamp;
    dword symbol_info[2];
    word size_optheader;
    word img_type;
    
    word magic;
    word linker_version;
    dword reserved1[3];
    dword entrance;
    dword codebase;
    qword imgbase;
    dword align_section;
    dword align_file;
    dword versions[4];
    
    dword imgsize;
    dword header_size;
    dword checksum;
    word subsystem;
    word attrib;
    qword stk_reserve;
    qword stk_commit;
    qword heap_reserve;
    qword heap_commit;
    dword reserved2;
    dword dir_count;
    struct{
        dword offset;
        dword size;
    }directory[0x10];
};

struct PE_SECTION{
    char name[8];
    dword size;
    dword offset;
    dword datasize;
    dword fileoffset;
    dword reserved[3];
    dword attrib;
};

void Disasm::load_image(const char* pe_name){
    do{
        ifstream img(pe_name,ios::binary);
        byte buffer[0x200] = {0};
        img.read((char*)buffer,0x200);
        if (!img.good())
            break;
        if (0x5A4D != *(word*)buffer)
            break;
        auto pe_info = (PE_INFO*)(buffer + *(dword*)(buffer + 0x3C));
        auto pe_section = (PE_SECTION*)(pe_info + 1);
        auto section_count = pe_info->section_count;

        if (0x4550 != pe_info->signature)
            break;
        if (0xF0 != pe_info->size_optheader)
            break;
        if (!pe_info->section_count)
			break;
        if (pe_info->dir_count != 0x10)
			break;
        
        base = pe_info->imgbase;

        while(section_count--){
            if (0x747865742EULL == *(qword*)pe_section->name){

                img.seekg(pe_section->fileoffset);
                if (!img.good())
                    break;

                code = new byte[pe_section->size];
                img.read((char*)code,pe_section->datasize);
                if (!img.good())
                    break;
                
                base += pe_section->offset;
                length = pe_section->size;
                return;
            }
            ++pe_section;
        }
        
    }while(false);
    throw runtime_error(string("Image load failed : ").append(pe_name));
}