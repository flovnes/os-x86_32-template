#include "console.h"

static unsigned short console_cursor_pos = 0;
u8 console_color = (VGA_COLOR_BLACK << 4) | VGA_COLOR_LIGHT_GREY;

static void console_put_cursor(unsigned short pos) {
    out(0x3D4, 14);
    out(0x3D5, ((pos >> 8) & 0x00FF));
    out(0x3D4, 15);
    out(0x3D5, pos & 0x00FF);
}

static void console_scroll() {
    char *framebuffer = (char *)VGA_ADDRESS;
    for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1) * 2; i++) {
        framebuffer[i] = framebuffer[i + VGA_WIDTH * 2];
    }
    for (int i = VGA_WIDTH * (VGA_HEIGHT - 1) * 2; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2) {
        framebuffer[i] = ' ';
        framebuffer[i + 1] = console_color;
    }
    console_cursor_pos = (VGA_HEIGHT - 1) * VGA_WIDTH;
    console_put_cursor(console_cursor_pos);
}

void console_set_color(u8 bg, u8 fg) {
    console_color = (bg << 4) | (fg & 0x0F);
}

void console_clear() {
    char *framebuffer = (char *)VGA_ADDRESS;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        framebuffer[i * 2] = ' ';
        framebuffer[i * 2 + 1] = console_color;
    }
    console_cursor_pos = 0;
    console_put_cursor(0);
}

void console_put_char(char c) {
    char *framebuffer = (char *)VGA_ADDRESS;
    if (c == '\n') {
        console_cursor_pos = (console_cursor_pos / VGA_WIDTH + 1) * VGA_WIDTH;
    } else if (c == '\b') {
        if (console_cursor_pos > 0) {
            console_cursor_pos--;
            framebuffer[console_cursor_pos * 2] = ' ';
            framebuffer[console_cursor_pos * 2 + 1] = console_color;
        }
    } else {
        framebuffer[console_cursor_pos * 2] = c;
        framebuffer[console_cursor_pos * 2 + 1] = console_color;
        console_cursor_pos++;
    }
    if (console_cursor_pos >= VGA_WIDTH * VGA_HEIGHT) {
        console_scroll();
    }
    console_put_cursor(console_cursor_pos);
}

void console_print(const char *s) {
    while (*s != '\0') {
        console_put_char(*s);
        s++;
    }
}

void console_show_cursor() {
    out(0x3D4, 0x0A);
    out(0x3D5, 0x0E);
}

void console_hide_cursor() {
    out(0x3D4, 0x0A);
    out(0x3D5, 0x20);
}


