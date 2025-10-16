#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include "kernel/kernel.h"

u32 strlen_custom(const char* str);
int strcmp(const char *s1, const char *s2);
char* strcpy_custom(char* dest, const char* src);
char* str_split(char* str, const char* delim);

#endif


