#ifndef TEXT_BUFFER_H
#define TEXT_BUFFER_H

#include "kernel/kernel.h"

typedef struct {
    char *data;               // backing storage provided by caller
    u32 capacity;             // max characters excluding trailing NUL
    u32 length;               // current length excluding trailing NUL
    u32 cursorIndex;          // insertion point in [0, length]
} TextBuffer;

void tb_init(TextBuffer *tb, char *storage, u32 capacity, const char *initialContent);
void tb_clear(TextBuffer *tb);

// Editing operations
void tb_insert_char(TextBuffer *tb, char c);   // handles regular chars and '\n'
void tb_backspace(TextBuffer *tb);             // delete char before cursor
void tb_insert_tab(TextBuffer *tb);            // insert 4 spaces

// Query helpers
u32 tb_length(const TextBuffer *tb);
u32 tb_cursor_index(const TextBuffer *tb);

#endif // TEXT_BUFFER_H


