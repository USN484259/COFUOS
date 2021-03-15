#pragma once
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
[[ noreturn ]]
#else
_Noreturn
#endif
void exit(int ret_val);
int isspace(int ch);
size_t strlen(const char* str);
unsigned long strtoul(const char* str,char** end,int base);
unsigned long long strtoull(const char* str,char** end,int base);
int snprintf(char* buffer,size_t limit,const char* format,...);

#ifdef __cplusplus
}
#endif