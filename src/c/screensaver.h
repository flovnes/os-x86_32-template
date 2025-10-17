#ifndef SCREENSAVER_H
#define SCREENSAVER_H

#include "kernel/kernel.h"
#include "console.h"

void screensaver_start();
void screensaver_tick();
void screensaver_stop(u8* saved_screen, bool saved_screen_valid, unsigned short saved_cursor_pos, unsigned short* current_cursor_pos);

#endif


