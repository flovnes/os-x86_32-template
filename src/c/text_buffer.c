#include "text_buffer.h"
#include <stddef.h>

static void tb_clamp_cursor(TextBuffer *tb) {
    if (tb->cursorIndex > tb->length) {
        tb->cursorIndex = tb->length;
    }
}

void tb_init(TextBuffer *tb, char *storage, u32 capacity, const char *initialContent) {
    tb->data = storage;
    tb->capacity = capacity;
    tb->length = 0;
    tb->cursorIndex = 0;
    tb_clear(tb);
    if (initialContent != NULL) {
        // Copy up to capacity characters
        u32 i = 0;
        while (initialContent[i] != '\0' && i < capacity) {
            tb->data[i] = initialContent[i];
            i++;
        }
        tb->length = i;
        tb->data[tb->length] = '\0';
        tb->cursorIndex = tb->length;
    }
}

void tb_clear(TextBuffer *tb) {
    tb->length = 0;
    tb->cursorIndex = 0;
    if (tb->data != 0 && tb->capacity > 0) {
        tb->data[0] = '\0';
    }
}

void tb_insert_char(TextBuffer *tb, char c) {
    if (tb->length >= tb->capacity) {
        return;
    }
    // shift right from cursor to make room
    for (u32 i = tb->length; i > tb->cursorIndex; i--) {
        tb->data[i] = tb->data[i - 1];
    }
    tb->data[tb->cursorIndex] = c;
    tb->length++;
    tb->cursorIndex++;
    tb->data[tb->length] = '\0';
}

void tb_backspace(TextBuffer *tb) {
    if (tb->cursorIndex == 0 || tb->length == 0) {
        return;
    }
    // delete char before cursor
    u32 delIndex = tb->cursorIndex - 1;
    for (u32 i = delIndex; i < tb->length; i++) {
        tb->data[i] = tb->data[i + 1];
    }
    tb->length--;
    tb->cursorIndex--;
    tb_clamp_cursor(tb);
    tb->data[tb->length] = '\0';
}

void tb_insert_tab(TextBuffer *tb) {
    // insert four spaces if capacity allows
    if (tb->length + 4 > tb->capacity) {
        return;
    }
    for (int i = 0; i < 4; i++) {
        tb_insert_char(tb, ' ');
    }
}

u32 tb_length(const TextBuffer *tb) {
    return tb->length;
}

u32 tb_cursor_index(const TextBuffer *tb) {
    return tb->cursorIndex;
}


