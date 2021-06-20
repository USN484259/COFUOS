#pragma once
#include "types.h"


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

typedef struct {
	enum : word{
		EOF_BIT = 1,
		FAIL_BIT = 2,
		BAD_BIT = 4,
	};
	HANDLE file;
	word flags;
	dword head;
	dword tail;
	char buffer[0x1F0];
} FILE;

extern FILE std_obj[3];

#define STDIN ((HANDLE)1)
#define STDOUT ((HANDLE)2)
#define STDERR ((HANDLE)3)

#define stdin (std_obj + 0)
#define stdout (std_obj + 1)
#define stderr (std_obj + 2)

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
const char* strstr(const char* str,const char* substr);
int strcmp(const char* a,const char* b);

unsigned long strtoul(const char* str,const char** end,int base);
unsigned long long strtoull(const char* str,const char** end,int base);
int snprintf(char* buffer,size_t limit,const char* format,...);
int fprintf(FILE* stream,const char* format,...);
#define printf(...) fprintf(stdout,__VA_ARGS__)
int puts(const char* str);

int fputs(const char* str,FILE* stream);
int fputc(int ch,FILE* stream);
#define putchar(ch) fputc((ch),stdout)

char* fgets(char* str,int count,FILE* stream);
int fgetc(FILE* stream);
#define getchar() fgetc(stdin)

int ungetc(int ch,FILE* stream);

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