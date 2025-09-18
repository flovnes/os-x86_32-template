#include "file_system.h"
#include "../../kernel/kernel.h"
#include "../serial_port/serial_port.h"
#include <stdbool.h>
#include <stddef.h>

static struct imfs_file imfs_files[MAX_FILES];

void init_imfs() {
    for (int i = 0; i < MAX_FILES; i++) {
        imfs_files[i].in_use = false;
        imfs_files[i].name[0] = '\0';
        imfs_files[i].content[0] = '\0';
        imfs_files[i].size = 0;
    }
}

u32 strlen_custom(const char* str) {
    u32 len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

char* strcpy_custom(char* dest, const char* src) {
    char* original_dest = dest;
    while ((*dest++ = *src++) != '\0');
    return original_dest;
}

static char* split_ptr = NULL;

char* str_split(char* str, const char* delim) {
    if (str != NULL) {
        split_ptr = str;
    } else if (split_ptr == NULL) {
        return NULL;
    }

    while (*split_ptr != '\0' && *split_ptr == *delim) {
        split_ptr++;
    }

    if (*split_ptr == '\0') {
        split_ptr = NULL;
        return NULL;
    }

    char* token_start = split_ptr;
    while (*split_ptr != '\0' && *split_ptr != *delim) {
        split_ptr++;
    }

    if (*split_ptr != '\0') {
        *split_ptr = '\0';
        split_ptr++;
    } else {
        split_ptr = NULL;
    }

    return token_start;
}

int find_file_index(const char *filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (imfs_files[i].in_use && strcmp(imfs_files[i].name, filename) == 0) {
            return i;
        }
    }
    return -1;
}

int find_free_file_index() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!imfs_files[i].in_use) {
            return i;
        }
    }
    return -1;
}

int create_file(const char *filename) {
    if (strlen_custom(filename) == 0 || strlen_custom(filename) > MAX_FILENAME_LENGTH) {
        return -1;
    }
    if (find_file_index(filename) != -1) {
        return -1;
    }
    int free_idx = find_free_file_index();
    if (free_idx == -1) {
        return -1;
    }

    struct imfs_file *file = &imfs_files[free_idx];
    strcpy_custom(file->name, filename);
    file->content[0] = '\0'; 
    file->size = 0;
    file->in_use = true;
    return 0;
}

int write_file(const char *filename, const char *data) {
    u32 data_len = strlen_custom(data);
    if (data_len > MAX_FILE_CONTENT_LENGTH) {
        return -1;
    }

    int file_idx = find_file_index(filename);
    if (file_idx == -1) {
        create_file(filename);
        file_idx = find_file_index(filename);
    }

    struct imfs_file *file = &imfs_files[file_idx];
    strcpy_custom(file->content, data);
    file->size = data_len;
    return 0;
}

const char* read_file(const char *filename) {
    int file_idx = find_file_index(filename);
    if (file_idx == -1) {
        return NULL;
    }
    return imfs_files[file_idx].content;
}

int delete_file(const char *filename) {
    int file_idx = find_file_index(filename);
    if (file_idx == -1) {
        return -1;
    }

    struct imfs_file *file = &imfs_files[file_idx];
    file->in_use = false;
    file->name[0] = '\0';    
    file->content[0] = '\0'; 
    file->size = 0;
    return 0;
}

void imfs_list_files() {
    bool found_files = false;
    for (int i = 0; i < MAX_FILES; i++) {
        if (imfs_files[i].in_use) {
            print_string(imfs_files[i].name);
            print_string(" (");
            char size_str[12]; 
            u32 temp_size = imfs_files[i].size;
            int j = 0;
            if (temp_size == 0) {
                size_str[j++] = '0';
            } else {
                while (temp_size > 0) {
                    size_str[j++] = (temp_size % 10) + '0';
                    temp_size /= 10;
                }
                for (int k = 0; k < j / 2; k++) {
                    char temp = size_str[k];
                    size_str[k] = size_str[j - 1 - k];
                    size_str[j - 1 - k] = temp;
                }
            }
            size_str[j] = '\0';

            print_string(size_str);
            print_string(" bytes)\n");
            found_files = true;
        }
    }
    if (!found_files) {
        print_string("No files found.\n");
    }
}