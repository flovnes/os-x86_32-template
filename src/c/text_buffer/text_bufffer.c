#include "text_buffer.h"
#include "../kernel/kernel.h"
#include "../../drivers/serial_port/serial_port.h"
#include <stddef.h>

extern u32 strlen_custom(const char* str);
extern char* strcpy_custom(char* dest, const char* src);

void text_buffer_init(struct text_buffer *tb, char *buf_ptr, u32 cap) {
    tb->buffer = buf_ptr;
    tb->capacity = cap;
    tb->length = 0;
    tb->cursor_idx = 0;
    tb->screen_x = 0;
    tb->screen_y = 0;
    if (tb->buffer != NULL && tb->capacity > 0) {
        tb->buffer[0] = '\0';
    }
}

void text_buffer_move_cursor_left(struct text_buffer *tb) {
    if (tb == NULL) return;
    if (tb->cursor_idx > 0) {
        tb->cursor_idx--;
    }
}

void text_buffer_move_cursor_right(struct text_buffer *tb) {
    if (tb == NULL) return;
    if (tb->cursor_idx < tb->length) {
        tb->cursor_idx++;
    }
}

void text_buffer_insert_char(struct text_buffer *tb, char c) {
    if (tb == NULL || tb->buffer == NULL || tb->length >= tb->capacity - 1) {
        return;
    }
    if (tb->cursor_idx > tb->length) {
        tb->cursor_idx = tb->length;
    }

    u32 chars_to_insert = 1;
    if (c == '\t') {
        chars_to_insert = 4;
    }

    if (tb->length + chars_to_insert >= tb->capacity) {
        return;
    }

    for (u32 i = tb->length; i >= tb->cursor_idx; i--) {
        tb->buffer[i + chars_to_insert] = tb->buffer[i];
    }

    if (c == '\t') {
        for (u32 i = 0; i < chars_to_insert; i++) {
            tb->buffer[tb->cursor_idx + i] = ' ';
        }
    } else {
        tb->buffer[tb->cursor_idx] = c;
    }

    tb->cursor_idx += chars_to_insert;
    tb->length += chars_to_insert;
    tb->buffer[tb->length] = '\0';
}

void text_buffer_delete_char(struct text_buffer *tb) {
    if (tb == NULL || tb->buffer == NULL || tb->cursor_idx == 0 || tb->length == 0) {
        return;
    }

    tb->cursor_idx--;

    for (u32 i = tb->cursor_idx; i < tb->length; i++) {
        tb->buffer[i] = tb->buffer[i + 1];
    }
    tb->length--;
    tb->buffer[tb->length] = '\0';
}

void text_buffer_clear(struct text_buffer *tb) {
    if (tb == NULL || tb->buffer == NULL) return;
    tb->buffer[0] = '\0';
    tb->length = 0;
    tb->cursor_idx = 0;
    tb->screen_x = 0;
    tb->screen_y = 0;
}

void text_buffer_set_content(struct text_buffer *tb, const char *content) {
    if (tb == NULL || tb->buffer == NULL) return;
    if (content == NULL) content = "";

    u32 content_len = strlen_custom(content);
    if (content_len >= tb->capacity) {
        content_len = tb->capacity - 1;
    }

    for (u32 i = 0; i < content_len; i++) {
        tb->buffer[i] = content[i];
    }
    tb->buffer[content_len] = '\0';
    tb->length = content_len;
    tb->cursor_idx = content_len;
    tb->screen_x = 0;
    tb->screen_y = 0; // Will be recalculated on redraw
}

const char* text_buffer_get_content(struct text_buffer *tb) {
    if (tb == NULL || tb->buffer == NULL) return "";
    return tb->buffer;
}

void text_buffer_redraw_screen(struct text_buffer *tb, u32 start_row, u32 start_col) {
    if (tb == NULL || tb->buffer == NULL) return;

    // Clear the screen (or the relevant portion) before drawing
    // For now, we'll clear the whole screen, similar to editor_refresh_screen
    clear_screen();

    char *framebuffer = (char *)VGA_ADDRESS;
    u32 current_screen_x = start_col;
    u32 current_screen_y = start_row;

    u32 target_cursor_screen_x = start_col;
    u32 target_cursor_screen_y = start_row;

    // Loop through the buffer content to draw and calculate cursor position
    for (u32 i = 0; i <= tb->length; i++) {
        // If we are at the buffer's cursor_idx, record the current screen_x/y
        if (i == tb->cursor_idx) {
            target_cursor_screen_x = current_screen_x;
            target_cursor_screen_y = current_screen_y;
        }

        if (i == tb->length) { // Reached end of buffer
            break; // Stop processing characters
        }

        char c = tb->buffer[i];

        if (c == '\n') {
            current_screen_x = start_col; // Reset to start_col for next line
            current_screen_y++;
        } else if (c == '\t') {
            u32 tab_stop = 4;
            current_screen_x = (current_screen_x + tab_stop) & ~(tab_stop - 1);
            if (current_screen_x >= VGA_WIDTH) { // Handle tab wrapping
                current_screen_x -= VGA_WIDTH;
                current_screen_y++;
            }
        } else {
            // Only draw if within screen bounds
            if (current_screen_y < VGA_HEIGHT && current_screen_x < VGA_WIDTH) {
                framebuffer[(current_screen_y * VGA_WIDTH + current_screen_x) * 2] = c;
                framebuffer[(current_screen_y * VGA_WIDTH + current_screen_x) * 2 + 1] = COLORS;
            }
            current_screen_x++;
            if (current_screen_x >= VGA_WIDTH) {
                current_screen_x = start_col; // Reset to start_col for next line
                current_screen_y++;
            }
        }

        // Basic scrolling: if content exceeds screen height, cap screen_y
        if (current_screen_y >= VGA_HEIGHT) {
            current_screen_y = VGA_HEIGHT - 1; // Cap screen_y to prevent drawing off screen
        }
    }

    // Update the text_buffer's internal screen cursor position
    tb->screen_x = target_cursor_screen_x;
    tb->screen_y = target_cursor_screen_y;

    // Update the hardware cursor
    put_cursor(tb->screen_y * VGA_WIDTH + tb->screen_x);
}