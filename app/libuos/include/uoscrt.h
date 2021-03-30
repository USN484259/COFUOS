#pragma once
#include "types.h"

#define stdin ((HANDLE)1)
#define stdout ((HANDLE)2)
#define stderr ((HANDLE)3)

#ifdef __cplusplus
#define NORETURN [[ noreturn ]]
#else
#define NORETURN _Noreturn
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE ((qword)0x1000)
#endif

#ifndef EOF
#define EOF (-1)
#endif

#ifdef __cplusplus
extern "C" {
#endif


NORETURN void exit(int ret_val);
NORETURN void abort(void);

void* malloc(size_t);
void free(void*);

int isspace(int ch);
int isupper(int ch);
int islower(int ch);
int isalpha(int ch);
int isdigit(int ch);
int isxdigit(int ch);
int isalnum(int ch);
int ispunct(int ch);
int isprint(int ch);

void* memset(void* dst,int val,size_t len);
void* memcpy(void* dst,const void* sor,size_t len);
void* zeromemory(void* dst,size_t len);
size_t strlen(const char* str);
unsigned long strtoul(const char* str,char** end,int base);
unsigned long long strtoull(const char* str,char** end,int base);
int snprintf(char* buffer,size_t limit,const char* format,...);
int fprintf(HANDLE stream,const char* format,...);
#define printf(...) fprintf(stdout,__VA_ARGS__)

int fputs(const char* str,HANDLE stream);
int fputc(int ch,HANDLE stream);
#define putchar(ch) fputc((ch),stdout)

#ifdef __cplusplus
}

void* operator new(size_t);
void* operator new(size_t,void*);
void operator delete(void*,size_t);

#endif

#ifndef NDEBUG
#define assert(e) ( (e) ? 0:(abort(),-1) )
#else
#define assert(e)
#endif