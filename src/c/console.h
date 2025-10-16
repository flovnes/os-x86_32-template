#ifndef CONSOLE_H
#define CONSOLE_H

#include "kernel/kernel.h"

#define VGA_COLOR_BLACK 0x0
#define VGA_COLOR_BLUE 0x1
#define VGA_COLOR_GREEN 0x2
#define VGA_COLOR_CYAN 0x3
#define VGA_COLOR_RED 0x4
#define VGA_COLOR_MAGENTA 0x5
#define VGA_COLOR_BROWN 0x6
#define VGA_COLOR_LIGHT_GREY 0x7

extern u8 console_color;

void console_set_color(u8 bg, u8 fg);
void console_clear();
void console_put_char(char c);
void console_print(const char *s);

#endif


