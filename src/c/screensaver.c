#include "screensaver.h"
#include "console.h"
#include "string.h"

extern void put_cursor(unsigned short pos);

#define MAX_FISH 6

struct fish {
    u32 x, y;
    u32 direction;
    u32 speed;
};

static struct fish fish_array[MAX_FISH];
static const char *fish_pattern = ">< '>";
static const char *fish_pattern_reverse = "<' ><";
static u32 pattern_len;

void screensaver_start() {
    console_clear();
    pattern_len = strlen_custom(fish_pattern);
    
    for (int i = 0; i < MAX_FISH; i++) {
        fish_array[i].x = (i * VGA_WIDTH) / MAX_FISH;
        fish_array[i].y = 2 + (i * 5) % (VGA_HEIGHT - 4);
        fish_array[i].direction = i % 2;
        fish_array[i].speed = 1 + i/3;
    }

    const char *message = " good night. ";
    unsigned short msg_len = 0;
    while(message[msg_len] != '\0') msg_len++;

    char *framebuffer = (char *)VGA_ADDRESS;
    unsigned short start_pos = (VGA_WIDTH * 2) + (VGA_WIDTH / 2) - (msg_len / 2 + 1);
    for (unsigned short i = 0; i < msg_len; i++) {
        framebuffer[(start_pos + i) * 2] = message[i];
        framebuffer[(start_pos + i) * 2 + 1] = console_color;
    }
}

void screensaver_tick() {
    char *framebuffer = (char *)VGA_ADDRESS;

    for (int f = 0; f < MAX_FISH; f++) {
        u32 offset = fish_array[f].y * VGA_WIDTH * 2 + fish_array[f].x * 2;
        for (u32 i = 0; i < pattern_len; i++) {
            if (fish_array[f].x + i < VGA_WIDTH && fish_array[f].y < VGA_HEIGHT) {
                framebuffer[offset + i * 2] = ' ';
                framebuffer[offset + i * 2 + 1] = console_color;
            }
        }
    }

    for (int f = 0; f < MAX_FISH; f++) {
        if (fish_array[f].direction == 0) {
            fish_array[f].x += fish_array[f].speed;
            if (fish_array[f].x >= VGA_WIDTH) {
                fish_array[f].x = 0;
                fish_array[f].y = (fish_array[f].y + 3) % VGA_HEIGHT;
            }
        } else {
            if (fish_array[f].x < fish_array[f].speed) {
                fish_array[f].x = VGA_WIDTH - 1;
                fish_array[f].y = (fish_array[f].y + 3) % VGA_HEIGHT;
            } else {
                fish_array[f].x -= fish_array[f].speed;
            }
        }
    }

    for (int f = 0; f < MAX_FISH; f++) {
        u32 offset = fish_array[f].y * VGA_WIDTH * 2 + fish_array[f].x * 2;
        const char *pattern = fish_array[f].direction == 0 ? fish_pattern : fish_pattern_reverse;
        for (u32 i = 0; i < pattern_len; i++) {
            if (fish_array[f].x + i < VGA_WIDTH && fish_array[f].y < VGA_HEIGHT) {
                framebuffer[offset + i * 2] = pattern[i];
                framebuffer[offset + i * 2 + 1] = console_color;
            }
        }
    }
}

void screensaver_stop(u8* saved_screen, bool saved_screen_valid, unsigned short saved_cursor_pos, unsigned short* current_cursor_pos) {
    console_show_cursor();
    
    if (saved_screen_valid) {
        char *framebuffer = (char *)VGA_ADDRESS;
        for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i++) {
            framebuffer[i] = saved_screen[i];
        }
        *current_cursor_pos = saved_cursor_pos;
        put_cursor(*current_cursor_pos);
    } else {
        console_clear();
    }
}


