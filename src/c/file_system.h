#ifndef fs_H
#define fs_H

#include "kernel/kernel.h"
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

void init_fs();
int create_file(const char *filename);
int write_file(const char *filename, const char *data);
const char* read_file(const char *filename); // const char* to content or NULL
int delete_file(const char *filename);
void fs_list_files();

// String utilities moved to lib/string.h

int find_file_index(const char *filename);
int find_free_file_index();

#endif