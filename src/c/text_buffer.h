#ifndef TEXT_BUFFER_H
#define TEXT_BUFFER_H

#include "kernel/kernel.h"

typedef struct {
    char *data;
    u32 capacity;
    u32 length;
    u32 cursorIndex;
} TextBuffer;

void tb_init(TextBuffer *tb, char *storage, u32 capacity, const char *initialContent);
void tb_clear(TextBuffer *tb);

void tb_insert_char(TextBuffer *tb, char c);
void tb_backspace(TextBuffer *tb);
void tb_insert_tab(TextBuffer *tb);

u32 tb_length(const TextBuffer *tb);
u32 tb_cursor_index(const TextBuffer *tb);

#endif


