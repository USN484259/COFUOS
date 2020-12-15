#include "lang.hpp"
#include "util.hpp"
#include "../cpu/include/hal.hpp"
#include "../memory/include/heap.hpp"
#include "assert.hpp"

using namespace UOS;

void* operator new(size_t len){
	if (!len)
		return nullptr;
	auto res = heap.allocate(len);
    if (!res)
        BugCheck(bad_alloc,len);
    return res;
}

void* operator new(size_t,void* pos){
	return pos;
}


void operator delete(void* p,size_t len){
	if (!p)
		return;
	heap.release(p,len);
}

void* operator new[](size_t len){
	qword* p=(qword*)operator new(len+sizeof(qword));
	*p=(qword)len;	//place size in the first qword of block
	return p+1;	//return the rest of block
}

void operator delete[](void* p){
	if (!p)
		return;
	qword* b=(qword*)p;
	--b;	//point back to the block head
	operator delete(b,*b);	//block size at head
}

template<typename T>
inline void copy_memory(byte* dst,const byte* sor,size_t count){
    auto aligned_dst = (T*)align_up((qword)dst,sizeof(T));
    assert((byte*)aligned_dst >= dst);
    auto off = (size_t)((byte*)aligned_dst - dst);
    if (count >= off){
        count -= off;
        auto aligned_count = count / sizeof(T);
        count &= sizeof(T) - 1;
        while (dst != (byte*)aligned_dst)
            *dst++ = *sor++;

        auto aligned_sor = (const T*)sor;
        while(aligned_count--)
            *aligned_dst++ = *aligned_sor++;

        dst = (byte*)aligned_dst;
        sor = (const byte*)aligned_sor;
    }
    while(count--)
        *dst++ = *sor++;
}

template<>
inline void copy_memory<byte>(byte* dst,const byte* sor,size_t count){
    while(count--)
        *dst++ = *sor++;
}

void UOS::zeromemory(void* dst,size_t count){
    auto ptr = (byte*)dst;
    auto aligned_ptr = (qword*)align_up((qword)dst,8);
    assert((byte*)aligned_ptr >= ptr);
    auto off = (size_t)((byte*)aligned_ptr - ptr);
    if (count >= off){
        count -= off;
        auto aligned_count = count >> 3;
        count &= 7;
        while(ptr != (byte*)aligned_ptr)
            *ptr++ = 0;
        while(aligned_count--){
            *aligned_ptr++ = 0;
        }
        ptr = (byte*)aligned_ptr;
    }
    while(count--)
        *ptr++ = 0;
}

extern "C" {
    //__chkstk in hal.asm

    int atexit(void (*)(void)){
        return 0;
    }

    int _purecall(void){
        BugCheck(assert_failed);
    }

    void* imp_memset(void* dst,int ch,size_t count){
        if (ch == 0)
            zeromemory(dst,count);
        else{
            for(byte* cur = (byte*)dst;cur < ((byte*)dst + count);++cur){
                *cur = (byte)ch;
            }
        }
        return dst;
    }

    void* imp_memcpy(void* dst,const void* sor,size_t count){
        auto align_level = ( (qword)dst ^ (qword)sor ) & 0x07;
        switch(align_level){
            case 0:     //qword
            copy_memory<qword>((byte*)dst,(const byte*)sor,count);
            break;
            case 4:     //dword
            copy_memory<dword>((byte*)dst,(const byte*)sor,count);
            break;
            case 2:     //word
            copy_memory<word>((byte*)dst,(const byte*)sor,count);
            break;
            default:    //byte
            copy_memory<byte>((byte*)dst,(const byte*)sor,count);
            break;
        }
        return dst;
    }
}