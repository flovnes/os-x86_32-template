#include "kernel/kernel.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/timer/timer.h"
#include "drivers/serial_port/serial_port.h"
#include "drivers/file_system/file_system.h"
#include <stdbool.h> 

enum kernel_mode {
    MODE_NORMAL,
    MODE_EDITOR,
    MODE_SCREENSAVER
};

static unsigned short current_cursor_pos = 0;
static unsigned short inactivity_counter = 0;
static const u32 SCREENSAVER_TIMEOUT_TICKS = 360;
static u32 timer_ticks = 0;
static enum kernel_mode current_mode = MODE_NORMAL;

static char editor_buffer[MAX_FILE_CONTENT_LENGTH + 1];
static u32 editor_buffer_idx = 0;
static u32 editor_cursor_x = 0;
static u32 editor_cursor_y = 0;
static char editor_filename[MAX_FILENAME_LENGTH + 1];

static int screensaver_string_x = 0;
static int screensaver_string_y = 0;
static int screensaver_dx = 1;
static int screensaver_dy = 1;
static u8 screensaver_color_idx = 0;
static const char *screensaver_string_right = ">< '>";
static const char *screensaver_string_left  = "<' ><";

static const char *hat_string_right = "  @ ";
static const char *hat_string_left  = " @  ";

static const u8 screensaver_colors[] = {
    0x0E, // yellow
    0x0A, // green
    0x0B, // blue
    0x0C, // red
    0x0D, // pink
    0x0F, // white
};
static const u32 num_screensaver_colors = sizeof(screensaver_colors) / sizeof(screensaver_colors[0]);

void scroll_screen(); 
void print_char(char c); 
void print_string(const char *s); 
void clear_screen(); 

#define MAX_COMMAND_LENGTH 256
static char command_buffer[MAX_COMMAND_LENGTH];
static u32 command_buffer_idx = 0;

void execute_command(char *command_line); 
void init_shell_prompt(); 

void exception_handler(u32 interrupt, u32 error, char *message) {
    serial_log(LOG_ERROR, message);
}

void init_kernel() {
    init_gdt();
    init_idt();
    init_exception_handlers();
    init_interrupt_handlers();
    register_timer_interrupt_handler();
    register_keyboard_interrupt_handler();
    configure_default_serial_port();
    set_exception_handler(exception_handler);
    enable_interrupts();
}

void put_cursor(unsigned short pos) {
    out(0x3D4, 14);
    out(0x3D5, ((pos >> 8) & 0x00FF));
    out(0x3D4, 15);
    out(0x3D5, pos & 0x00FF);
}

void scroll_screen() {
    char *framebuffer = (char *)VGA_ADDRESS;
    // move all lines up by 1
    // line is VGA_WIDTH, and each character is 2 bytes
    for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1) * 2; i++) {
        framebuffer[i] = framebuffer[i + VGA_WIDTH * 2];
    }
    // clear the last line
    for (int i = VGA_WIDTH * (VGA_HEIGHT - 1) * 2; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2) {
        framebuffer[i] = ' ';
        framebuffer[i + 1] = COLORS;
    }
    current_cursor_pos = (VGA_HEIGHT - 1) * VGA_WIDTH;
    put_cursor(current_cursor_pos);
}

void print_char(char c) {
    char *framebuffer = (char *)VGA_ADDRESS;
    if (c == '\n') {
        current_cursor_pos = (current_cursor_pos / VGA_WIDTH + 1) * VGA_WIDTH;
    } else if (c == '\b') {
        if (current_cursor_pos > 0) {
            current_cursor_pos--;
            framebuffer[current_cursor_pos * 2] = ' ';
            framebuffer[current_cursor_pos * 2 + 1] = COLORS;
        }
    } else {
        framebuffer[current_cursor_pos * 2] = c;
        framebuffer[current_cursor_pos * 2 + 1] = COLORS;
        current_cursor_pos++;
    }
    if (current_cursor_pos >= VGA_WIDTH * VGA_HEIGHT) {
        scroll_screen();
    }
    put_cursor(current_cursor_pos);
}

void print_string(const char *s) {
    while (*s != '\0') {
        print_char(*s);
        s++;
    }
}

void clear_screen() {
    char *framebuffer = (char *)VGA_ADDRESS;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        framebuffer[i * 2] = ' ';
        framebuffer[i * 2 + 1] = COLORS;
    }
    current_cursor_pos = 0;
    put_cursor(0);
}

_Noreturn void halt_loop() {
    while (1) { halt(); }
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void execute_command(char *command_line) {
    inactivity_counter = 0;

    char command_line_copy[MAX_COMMAND_LENGTH];
    strcpy_custom(command_line_copy, command_line);

    char *command = str_split(command_line_copy, " ");
    char *arg1 = str_split(NULL, " ");
    char *arg2 = str_split(NULL, "");

    if (command == NULL || *command == '\0') {
        // nothing
    } else if (strcmp(command, "clear") == 0 || strcmp(command, "cls") == 0) {
        clear_screen();
    } else if (strcmp(command, "help") == 0 || strcmp(command, "man") == 0) {
        print_string("  o help - You're here!\n");
        print_string("    clear\n");
        print_string("    sleep\n");
        print_string("    ls\n");
        print_string("    create <file_name>\n");
        print_string("    write <file_name> <content>\n");
        print_string("    edit <file_name>\n");
        print_string("    read <file_name>\n");
        print_string("    delete <file_name>\n");
        print_string("    say <text>\n");
    } else if (strcmp(command, "sleep") == 0 || strcmp(command, "gn") == 0) {
        activate_screensaver();
    }
    else if (strcmp(command, "ls") == 0) {
        list_files();
    } else if (strcmp(command, "create") == 0 || strcmp(command, "touch") == 0) {
        if (arg1 == NULL) {
            print_string("? say 'help'\n");
        } else {
            if (create_file(arg1) == 0) {
                print_string("File '");
                print_string(arg1);
                print_string("' created.\n");
            } else {
                print_string("Failed to create file '");
                print_string(arg1);
                print_string("'.\n");
            }
        }
    } else if (strcmp(command, "write") == 0) {
        if (arg1 == NULL || arg2 == NULL) {
            print_string("? say 'help'\n");
        } else {
            if (write_file(arg1, arg2) == 0) {
                print_string("Content written to '");
                print_string(arg1);
                print_string("'.\n");
            } else {
                print_string("Failed to write to file '");
                print_string(arg1);
                print_string("'.\n");
            }
        }
    } else if (strcmp(command, "edit") == 0 || strcmp(command, "v") == 0) {
        if (arg1 == NULL) {
            print_string("? say 'help'\n");
        } else if (strlen_custom(arg1) > MAX_FILENAME_LENGTH) {
            print_string("Error: Filename too long. Max 32 chars).\n");
        } else {
            const char *initial_content = read_file(arg1);
            if (initial_content == NULL) {
                initial_content = "\n";
            }
            editor_init(arg1, initial_content);
        }
    }
    else if (strcmp(command, "read") == 0 || strcmp(command, "cat") == 0) {
        if (arg1 == NULL) {
            print_string("? say 'help'\n");
        } else {
            const char *content = read_file(arg1);
            if (content != NULL) {
                print_string(content);
                print_char('\n');
            } else {
                print_string("File '");
                print_string(arg1);
                print_string("' not found.\n");
            }
        }
    } else if (strcmp(command, "delete") == 0 || strcmp(command, "rm") == 0) {
        if (arg1 == NULL) {
            print_string("? say 'help'\n");
        } else {
            if (delete_file(arg1) == 0) {
                print_string("File '");
                print_string(arg1);
                print_string("' deleted.\n");
            } else {
                print_string("Failed to delete file '");
                print_string(arg1);
                print_string("'.\n");
            }
        }
    }
    else if (strcmp(command, "say") == 0 || strcmp(command, "echo") == 0) {
        if (arg1 != NULL) {
            print_string(arg1);
        }
        print_char('\n');
    }
    else {
        print_string("\n  ? say 'help'\n");
    }
}

void init_shell_prompt() {
    if (current_mode != MODE_NORMAL) {return;}

    print_string("\n > ");
    for (u32 i = 0; i < command_buffer_idx; i++) {
        print_char(command_buffer[i]);
    }
}

void init_shell() {
    clear_screen();
    print_string("\n >< '>   le fish au chocolat \n");
    init_shell_prompt();
    command_buffer_idx = 0;
}

void key_handler(struct keyboard_event event) {
    switch (current_mode) {
        case MODE_SCREENSAVER:
            if (event.type == EVENT_KEY_PRESSED) {
                inactivity_counter = 0;
                current_mode = MODE_NORMAL;
                out(0x3D4, 0x0A); 
                out(0x3D5, 0x0E);
                clear_screen();
                init_shell_prompt();
            } 
        break;

        case MODE_EDITOR:
            if (event.type == EVENT_KEY_PRESSED) {
                if (event.key == KEY_ESC) {
                    editor_exit();
                } else if (event.key == KEY_F2) {
                    if (strlen_custom(editor_filename) > 0) {
                        if (write_file(editor_filename, editor_buffer) == 0) {
                            // cool
                        } else {
                            // error
                        }
                    } else {
                        // no save name
                    }
                    editor_exit();
                } else if (event.key == KEY_BACKSPACE) {
                    editor_put_char('\b');
                } else if (event.key == KEY_ENTER) {
                    editor_put_char('\n');
                } else if (event.key == KEY_TAB) {
                    editor_put_char('\t');
                } else if (event.key_character >= ' ' && event.key_character <= '~') {
                    editor_put_char(event.key_character);
                }
            }
        break;

        case MODE_NORMAL:
            if (event.type == EVENT_KEY_PRESSED) {
                if (event.key_character >= ' ' && event.key_character <= '~' && command_buffer_idx < MAX_COMMAND_LENGTH - 1) {
                    command_buffer[command_buffer_idx++] = event.key_character;
                    print_char(event.key_character);
                } else if (event.key == KEY_BACKSPACE) {
                    if (command_buffer_idx > 0) {
                        command_buffer_idx--;
                        print_char('\b');
                    }
                } else if (event.key == KEY_ENTER) {
                    print_char('\n');
                    command_buffer[command_buffer_idx] = '\0';
                    execute_command(command_buffer);
                    command_buffer_idx = 0;
                    init_shell_prompt();
                }
            }
        break;
    }
}

void timer_tick_handler() {
    timer_ticks++;
    if (current_mode == MODE_SCREENSAVER) {
        if (timer_ticks % 2 != 0) {
            return;
        }

        char *framebuffer = (char *)VGA_ADDRESS;
        u32 pattern_len = strlen_custom(screensaver_string_right);
        u8 current_color_attribute = (0x0 << 4) | screensaver_colors[screensaver_color_idx];

         u32 old_fish_offset = (screensaver_string_y * VGA_WIDTH + screensaver_string_x) * 2;
        if (screensaver_string_y > 0) {
            u32 old_hat_offset = ((screensaver_string_y - 1) * VGA_WIDTH + screensaver_string_x) * 2;
            for (u32 i = 0; i < pattern_len; i++) {
                if (screensaver_string_x + i < VGA_WIDTH) {
                    framebuffer[old_hat_offset + i * 2] = ' ';
                }
            }
        }
        for (u32 i = 0; i < pattern_len; i++) {
            if (screensaver_string_x + i < VGA_WIDTH) {
                framebuffer[old_fish_offset + i * 2] = ' ';
                framebuffer[old_fish_offset + i * 2 + 1] = COLORS;
            }
        }

        screensaver_string_x += screensaver_dx;
        screensaver_string_y += screensaver_dy;

        bool bounced = false;
        if (screensaver_string_x <= 0) {
            screensaver_string_x = 0;
            screensaver_dx = 1;
            bounced = true;
        } else if (screensaver_string_x + pattern_len >= VGA_WIDTH) {
            screensaver_string_x = VGA_WIDTH - pattern_len;
            screensaver_dx = -1;
            bounced = true;
        }

        if (screensaver_string_y <= 0) { 
            screensaver_string_y = 1;
            screensaver_dy = 1;
            bounced = true;
        } else if (screensaver_string_y >= VGA_HEIGHT - 1) {
            screensaver_string_y = VGA_HEIGHT - 1;
            screensaver_dy = -1;
            bounced = true;
        }
        
        if (bounced) {
            screensaver_color_idx = (screensaver_color_idx + 1) % num_screensaver_colors;
        }

        const char *current_fish_string = (screensaver_dx == -1) ? screensaver_string_right : screensaver_string_left;
        const char *current_hat_string = (screensaver_dx == -1) ? hat_string_right : hat_string_left;
        
        u32 new_hat_offset = ((screensaver_string_y - 1) * VGA_WIDTH + screensaver_string_x) * 2;
        for (u32 i = 0; i < pattern_len; i++) {
            if (screensaver_string_x + i < VGA_WIDTH) {
                framebuffer[new_hat_offset + i * 2] = current_hat_string[i];
                framebuffer[new_hat_offset + i * 2 + 1] = current_color_attribute; 
            }
        }

        u32 new_fish_offset = (screensaver_string_y * VGA_WIDTH + screensaver_string_x) * 2;
        for (u32 i = 0; i < pattern_len; i++) {
            if (screensaver_string_x + i < VGA_WIDTH) {
                framebuffer[new_fish_offset + i * 2] = current_fish_string[i];
                framebuffer[new_fish_offset + i * 2 + 1] = current_color_attribute; 
            }
        }

    } else if (current_mode == MODE_NORMAL) {
        inactivity_counter++;
        if (inactivity_counter >= SCREENSAVER_TIMEOUT_TICKS) {
            activate_screensaver();
            inactivity_counter = 0;
        }
    }
}

void activate_screensaver() {
    current_mode = MODE_SCREENSAVER;
    clear_screen(); 
    out(0x3D4, 0x0A);
    out(0x3D5, 0x20);

    screensaver_string_x = VGA_WIDTH / 2;
    screensaver_string_y = VGA_HEIGHT / 2; 
    screensaver_dx = 1;
    screensaver_dy = 1;
    screensaver_color_idx = 0; 
}

void kernel_entry() {
    init_kernel();
    keyboard_set_handler(key_handler);
    timer_set_handler(timer_tick_handler);

    init_shell();

    halt_loop();
}

static void editor_update_hw_cursor() {
    put_cursor(editor_cursor_y * VGA_WIDTH + editor_cursor_x);
}

void editor_init(const char *filename, const char *initial_content) {
    current_mode = MODE_EDITOR;
    strcpy_custom(editor_filename, filename);

    editor_buffer[0] = '\0';
    editor_buffer_idx = 0;
    if (initial_content != NULL) {
        u32 content_len = strlen_custom(initial_content);
        if (content_len > MAX_FILE_CONTENT_LENGTH) {
            content_len = MAX_FILE_CONTENT_LENGTH;
        }
        for(u32 i = 0; i < content_len; i++) {
            editor_buffer[i] = initial_content[i];
        }
        editor_buffer[content_len] = '\0';
        editor_buffer_idx = content_len;
    }

    editor_refresh_screen();
}

void editor_refresh_screen() {
    clear_screen();
    char *framebuffer = (char *)VGA_ADDRESS;
    u32 screen_x = 0;
    u32 screen_y = 0;

    u32 target_cursor_screen_x = 0;
    u32 target_cursor_screen_y = 0;

    u32 buffer_len = strlen_custom(editor_buffer);

    for (u32 i = 0; i <= buffer_len; i++) {
        if (i == editor_buffer_idx) {
            target_cursor_screen_x = screen_x;
            target_cursor_screen_y = screen_y;
        }

        if (i == buffer_len) {
            break;
        }

        char c = editor_buffer[i];

        if (c == '\n') {
            screen_x = 0;
            screen_y++;
        } else if (c == '\t') {
            u32 tab_stop = 4;
            screen_x = (screen_x + tab_stop) & ~(tab_stop - 1);
            if (screen_x >= VGA_WIDTH) {
                screen_x -= VGA_WIDTH;
                screen_y++;
            }
        } else {
            if (screen_y < VGA_HEIGHT) {
                framebuffer[(screen_y * VGA_WIDTH + screen_x) * 2] = c;
                framebuffer[(screen_y * VGA_WIDTH + screen_x) * 2 + 1] = COLORS;
            }
            screen_x++;
            if (screen_x >= VGA_WIDTH) {
                screen_x = 0;
                screen_y++;
            }
        }

        if (screen_y >= VGA_HEIGHT) {
            screen_y = VGA_HEIGHT - 1;
        }
    }

    editor_cursor_x = target_cursor_screen_x;
    editor_cursor_y = target_cursor_screen_y;

    editor_update_hw_cursor();
}

void editor_put_char(char c) {
    u32 current_buffer_len = strlen_custom(editor_buffer);

    if (c == '\b') {
        if (editor_buffer_idx > 0) {    
            editor_buffer_idx--;
            for (u32 i = editor_buffer_idx; i < current_buffer_len; i++) {
                editor_buffer[i] = editor_buffer[i + 1];
            }
            editor_buffer[current_buffer_len - 1] = '\0';
        }
    } else if (c == '\t') {
        if (current_buffer_len + 4 <= MAX_FILE_CONTENT_LENGTH) {
            for (u32 i = current_buffer_len; i >= editor_buffer_idx; i--) {
                editor_buffer[i + 4] = editor_buffer[i];
            }
            editor_buffer[editor_buffer_idx++] = ' ';
            editor_buffer[editor_buffer_idx++] = ' ';
            editor_buffer[editor_buffer_idx++] = ' ';
            editor_buffer[editor_buffer_idx++] = ' ';
            editor_buffer[current_buffer_len + 4] = '\0';
        }
    } else if (current_buffer_len < MAX_FILE_CONTENT_LENGTH) {
        for (u32 i = current_buffer_len; i >= editor_buffer_idx; i--) {
            editor_buffer[i + 1] = editor_buffer[i];
        }
        editor_buffer[editor_buffer_idx++] = c;
        editor_buffer[current_buffer_len + 1] = '\0';
    }

    editor_refresh_screen();
}

void editor_exit() {
    current_mode = MODE_NORMAL;
    editor_buffer[0] = '\0';
    editor_buffer_idx = 0;
    editor_cursor_x = 0;
    editor_cursor_y = 0;
    editor_filename[0] = '\0';

    // show cursor
    out(0x3D4, 0x0A);
    out(0x3D5, 0x0E);

    clear_screen();
    init_shell_prompt();
}


// TODO
// 1. Animate screensaver
//// 2. File editor
//    2.1 fix editor weird bug
// 3. Remember the screen before screensaving