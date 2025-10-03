#ifndef IMFS_H
#define IMFS_H

#include "../kernel/kernel.h"
#include <stdbool.h>
#include <stddef.h>

#define MAX_FILES               10
#define MAX_FILE_CONTENT_LENGTH 2000
#define MAX_FILENAME_LENGTH     32

struct fs_file {
    char name[MAX_FILENAME_LENGTH + 1];
    char content[MAX_FILE_CONTENT_LENGTH + 1];
    bool in_use;
    u32 size;
};

void init_file_system();
int create_file(const char *filename);
int write_file(const char *filename, const char *data);
const char* read_file(const char *filename); // const char* to content or NULL
int delete_file(const char *filename);
void list_files();

u32 strlen_custom(const char* str);
int strcmp(const char *s1, const char *s2);
char* strcpy_custom(char* dest, const char* src);
char* str_split(char* str, const char* delim);

int find_file_index(const char *filename);
int find_free_file_index();

#endif