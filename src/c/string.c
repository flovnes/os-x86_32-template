#include "string.h"

u32 strlen_custom(const char* str) {
    u32 len = 0;
    while (str[len] != '\0') { len++; }
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strcpy_custom(char* dest, const char* src) {
    char* original_dest = dest;
    while ((*dest++ = *src++) != '\0');
    return original_dest;
}

static char* split_ptr = 0;

char* str_split(char* str, const char* delim) {
    if (str != 0) {
        split_ptr = str;
    } else if (split_ptr == 0) {
        return 0;
    }

    while (*split_ptr != '\0' && *split_ptr == *delim) { split_ptr++; }
    if (*split_ptr == '\0') { split_ptr = 0; return 0; }

    char* token_start = split_ptr;
    while (*split_ptr != '\0' && *split_ptr != *delim) { split_ptr++; }

    if (*split_ptr != '\0') { *split_ptr = '\0'; split_ptr++; } else { split_ptr = 0; }
    return token_start;
}


