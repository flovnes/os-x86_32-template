#ifndef TEXT_BUFFER_H
#define TEXT_BUFFER_H

#include "../kernel/kernel.h"
#include <stdbool.h>

u32 strlen_custom(const char* str);
char* strcpy_custom(char* dest, const char* src);

struct text_buffer {
    char *buffer;
    u32 capacity;
    u32 length;
    u32 cursor_idx;

    u32 screen_x;
    u32 screen_y;
};

void text_buffer_init(struct text_buffer *tb, char *buf_ptr, u32 cap);

void text_buffer_move_cursor_left(struct text_buffer *tb);

void text_buffer_move_cursor_right(struct text_buffer *tb);

void text_buffer_insert_char(struct text_buffer *tb, char c);

void text_buffer_delete_char(struct text_buffer *tb);

void text_buffer_clear(struct text_buffer *tb);

void text_buffer_set_content(struct text_buffer *tb, const char *content);

const char* text_buffer_get_content(struct text_buffer *tb);

void text_buffer_redraw_screen(struct text_buffer *tb, u32 start_row, u32 start_col);

#endif