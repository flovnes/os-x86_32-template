#include "screensaver.h"
#include "console.h"
#include "string.h"

static const char *screensaver_string = "><< '>>"; // playful fish
static u32 screensaver_string_x = 0;
static u32 screensaver_string_y = 0;

void screensaver_start() {
    console_clear();
    screensaver_string_x = 0;
    screensaver_string_y = VGA_HEIGHT / 2;

    const char *message = " good night. ";
    unsigned short msg_len = 0;
    while(message[msg_len] != '\0') msg_len++;

    char *framebuffer = (char *)VGA_ADDRESS;
    unsigned short start_pos = (VGA_WIDTH * (VGA_HEIGHT / 2)) + (VGA_WIDTH / 2) - (msg_len / 2 + 1);
    for (unsigned short i = 0; i < msg_len; i++) {
        framebuffer[(start_pos + i) * 2] = message[i];
        framebuffer[(start_pos + i) * 2 + 1] = console_color;
    }
}

void screensaver_tick() {
    char *framebuffer = (char *)VGA_ADDRESS;
    u32 pattern_len = strlen_custom(screensaver_string);

    u32 prev_pattern_offset = screensaver_string_y * VGA_WIDTH * 2 + screensaver_string_x * 2;
    for (u32 i = 0; i < pattern_len; i++) {
        if (screensaver_string_x + i < VGA_WIDTH && screensaver_string_y < VGA_HEIGHT) {
            framebuffer[prev_pattern_offset + i * 2] = ' ';
            framebuffer[prev_pattern_offset + i * 2 + 1] = console_color;
        }
    }

    screensaver_string_x++;
    if (screensaver_string_x >= VGA_WIDTH) { screensaver_string_x = 0; }

    u32 current_pattern_offset = screensaver_string_y * VGA_WIDTH * 2 + screensaver_string_x * 2;
    for (u32 i = 0; i < pattern_len; i++) {
        if (screensaver_string_x + i < VGA_WIDTH && screensaver_string_y < VGA_HEIGHT) {
            framebuffer[current_pattern_offset + i * 2] = screensaver_string[i];
            framebuffer[current_pattern_offset + i * 2 + 1] = console_color;
        }
    }
}

void screensaver_stop() {
    // nothing special
}


