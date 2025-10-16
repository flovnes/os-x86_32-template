#include "file_system.h"
#include "kernel/kernel.h"
#include "drivers/serial_port/serial_port.h"
#include "string.h"
#include <stdbool.h>
#include <stddef.h>

static struct fs_file fs_files[MAX_FILES];

void init_fs() {
    for (int i = 0; i < MAX_FILES; i++) {
        fs_files[i].in_use = false;
        fs_files[i].name[0] = '\0';
        fs_files[i].content[0] = '\0';
        fs_files[i].size = 0;
    }
}


int find_file_index(const char *filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_files[i].in_use && strcmp(fs_files[i].name, filename) == 0) {
            return i;
        }
    }
    return -1;
}

int find_free_file_index() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs_files[i].in_use) {
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

    struct fs_file *file = &fs_files[free_idx];
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

    struct fs_file *file = &fs_files[file_idx];
    strcpy_custom(file->content, data);
    file->size = data_len;
    return 0;
}

const char* read_file(const char *filename) {
    int file_idx = find_file_index(filename);
    if (file_idx == -1) {
        return NULL;
    }
    return fs_files[file_idx].content;
}

int delete_file(const char *filename) {
    int file_idx = find_file_index(filename);
    if (file_idx == -1) {
        return -1;
    }

    struct fs_file *file = &fs_files[file_idx];
    file->in_use = false;
    file->name[0] = '\0';    
    file->content[0] = '\0'; 
    file->size = 0;
    return 0;
}

void fs_list_files() {
    bool found_files = false;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_files[i].in_use) {
            print_string(fs_files[i].name);
            print_string(" (");
            char size_str[12]; 
            u32 temp_size = fs_files[i].size;
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