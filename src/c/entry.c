#include "kernel/kernel.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/timer/timer.h"
#include "drivers/serial_port/serial_port.h"
#include "file_system.h"
#include "text_buffer.h"
#include "string.h"
#include <stdbool.h> 

enum kernel_mode {
    MODE_NORMAL,     // shell
    MODE_EDITOR,
    MODE_SCREENSAVER
};

static unsigned short current_cursor_pos = 0;
static unsigned short inactivity_counter = 0;
static const u32 SCREENSAVER_TIMEOUT_TICKS = 360;
static u32 timer_ticks = 0;
static enum kernel_mode current_mode = MODE_NORMAL;

static char editor_storage[MAX_FILE_CONTENT_LENGTH + 1];
static TextBuffer editor_tb;
static u32 editor_cursor_x = 0;
static u32 editor_cursor_y = 0;
static char editor_filename[MAX_FILENAME_LENGTH + 1];

static u32 screensaver_string_x = 0;
static u32 screensaver_string_y = 0;
static u8 screensaver_color_idx = 0;
static const char *screensaver_string = ">< '>";

void scroll_screen(); 
void print_char(char c); 
void print_string(const char *s); 
void clear_screen(); 
void activate_screensaver();
void editor_init(const char *filename, const char *initial_content);
void editor_refresh_screen();
void editor_put_char(char c);
void editor_exit();

#define MAX_ALLOCS 16
struct alloc { bool in_use; u32 id; u32 size; };
static struct alloc allocs[MAX_ALLOCS];
static u32 next_id = 1;

static void print_u32_dec(u32 value) {
    char buf[12];
    int i = 0;
    if (value == 0) { buf[i++] = '0'; }
    while (value > 0) { buf[i++] = (value % 10) + '0'; value /= 10; }
    for (int j = i - 1; j >= 0; j--) { print_char(buf[j]); }
}

static bool parse_u32_dec(const char *s, u32 *out_value) {
    if (s == NULL || *s == '\0') { return false; }
    u32 value = 0;
    const char *p = s;
    while (*p) {
        if (*p < '0' || *p > '9') { return false; }
        u32 digit = (u32)(*p - '0');
        u32 new_value = value * 10 + digit;
        if (new_value < value) { return false; }
        value = new_value;
        p++;
    }
    *out_value = value;
    return true;
}

#define MAX_COMMAND_LENGTH 256
static char command_storage[MAX_COMMAND_LENGTH];
static TextBuffer command_tb;

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
        current_cursor_pos = (current_cursor_pos / VGA_WIDTH + 1) * VGA_WIDTH; // a/n+1, a<=n
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
        print_string(" o help - You're here!\n");
        print_string("   clear\n");
        print_string("   sleep\n");
        print_string("   ls\n");
        print_string("   create <file_name>\n");
        print_string("   write <file_name> <content>\n");
        print_string("   edit <file_name>\n");
        print_string("   read <file_name>\n");
        print_string("   delete <file_name>\n");
        print_string("   say <text>\n");
        print_string("   malloc <size>\n");
        print_string("   free <id>\n");
    } else if (strcmp(command, "sleep") == 0 || strcmp(command, "gn") == 0) {
        activate_screensaver();
    }
    else if (strcmp(command, "malloc") == 0) {
        u32 size = 0;
        if (!parse_u32_dec(arg1, &size)) {
            print_string("Usage: malloc <size>\n");
        } else {
            int slot = -1;
            for (int i = 0; i < MAX_ALLOCS; i++) { if (!allocs[i].in_use) { slot = i; break; } }
            if (slot == -1) {
                print_string("No slots left\n");
            } else {
                allocs[slot].in_use = true;
                allocs[slot].id = next_id++;
                allocs[slot].size = size;
                print_string("id=");
                print_u32_dec(allocs[slot].id);
                print_string(" size=");
                print_u32_dec(size);
                print_char('\n');
            }
        }
    }
    else if (strcmp(command, "free") == 0) {
        u32 id = 0;
        if (!parse_u32_dec(arg1, &id)) {
            print_string("say 'help'\n");
        } else {
            bool found = false;
            for (int i = 0; i < MAX_ALLOCS; i++) {
                if (allocs[i].in_use && allocs[i].id == id) {
                    allocs[i].in_use = false;
                    allocs[i].id = 0;
                    allocs[i].size = 0;
                    found = true;
                    break;
                }
            }
            if (!found) {
                print_string("Unknown id\n");
            }
        }
    }
    else if (strcmp(command, "ls") == 0) {
        fs_list_files();
    } else if (strcmp(command, "create") == 0 || strcmp(command, "touch") == 0) {
        if (arg1 == NULL) {
            print_string("say 'help'\n");
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
            print_string("say 'help'\n");
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
            print_string("say 'help'\n");
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
            print_string("say 'help'\n");
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
            print_string("say 'help'\n");
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
        print_string("\n  ? Unknown command\n");
    }
}

void init_shell_prompt() {
    if (current_mode != MODE_NORMAL) {return;}

    print_string("\n > ");
    print_string(command_tb.data);
}

void init_shell() {
    clear_screen();
    print_string("\n >< '>   le fish au chocolat \n");
    init_shell_prompt();
    tb_clear(&command_tb);
}

void key_handler(struct keyboard_event event) {
    switch (current_mode) {
        case MODE_SCREENSAVER:
            if (event.type == EVENT_KEY_PRESSED) {
                inactivity_counter = 0;
                current_mode = MODE_NORMAL;
                out(0x3D4, 0x0A); 
                out(0x3D5, 0x0E); // show cursor
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
                        if (write_file(editor_filename, editor_tb.data) == 0) {
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
                if (event.key_character >= ' ' && event.key_character <= '~') {
                    if (tb_length(&command_tb) < MAX_COMMAND_LENGTH - 1) {
                        tb_insert_char(&command_tb, event.key_character);
                        print_char(event.key_character);
                    }
                } else if (event.key == KEY_BACKSPACE) {
                    if (tb_length(&command_tb) > 0) {
                        tb_backspace(&command_tb);
                        print_char('\b');
                    }
                } else if (event.key == KEY_ENTER) {
                    print_char('\n');
                    execute_command(command_tb.data);
                    tb_clear(&command_tb);
                    init_shell_prompt();
                }
            }
        break;
    }
}

void timer_tick_handler() {
    timer_ticks++;
    if (current_mode == MODE_SCREENSAVER) {
        // if (timer_ticks % 18 != 0) {return;}
        char *framebuffer = (char *)VGA_ADDRESS;
        u32 pattern_len = strlen_custom(screensaver_string);

        u32 prev_pattern_offset = screensaver_string_y * VGA_WIDTH * 2 + screensaver_string_x * 2;
        for (u32 i = 0; i < pattern_len; i++) {
            if (screensaver_string_x + i < VGA_WIDTH && screensaver_string_y < VGA_HEIGHT) {
                framebuffer[prev_pattern_offset + i * 2] = ' '; 
                framebuffer[prev_pattern_offset + i * 2 + 1] = COLORS; 
            }
        }

        screensaver_string_x++;
        if (screensaver_string_x >= VGA_WIDTH) { 
            screensaver_string_x = 0; 
        }

        u32 current_pattern_offset = screensaver_string_y * VGA_WIDTH * 2 + screensaver_string_x * 2;
        for (u32 i = 0; i < pattern_len; i++) {
            if (screensaver_string_x + i < VGA_WIDTH && screensaver_string_y < VGA_HEIGHT) {
                framebuffer[current_pattern_offset + i * 2] = screensaver_string[i];
                framebuffer[current_pattern_offset + i * 2 + 1] = COLORS; 
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

    screensaver_string_x = 0;
    screensaver_string_y = VGA_HEIGHT / 2; 
    screensaver_color_idx = 1; 

    char *framebuffer = (char *)VGA_ADDRESS;

    const char *message = " good night. ";
    unsigned short msg_len = 0;
    while(message[msg_len] != '\0') msg_len++;

    unsigned short start_pos = (VGA_WIDTH * (VGA_HEIGHT / 2)) + (VGA_WIDTH / 2) - (msg_len / 2 + 1); 
    for (unsigned short i = 0; i < msg_len; i++) {
        framebuffer[(start_pos + i) * 2] = message[i];
        framebuffer[(start_pos + i) * 2 + 1] = COLORS;
    }
}

void kernel_entry() {
    init_kernel();
    keyboard_set_handler(key_handler);
    timer_set_handler(timer_tick_handler);

    // initialize text buffers
    tb_init(&command_tb, command_storage, MAX_COMMAND_LENGTH - 1, NULL);

    init_shell();

    halt_loop();
}

static void editor_update_hw_cursor() {
    put_cursor(editor_cursor_y * VGA_WIDTH + editor_cursor_x);
}

void editor_init(const char *filename, const char *initial_content) {
    current_mode = MODE_EDITOR;
    strcpy_custom(editor_filename, filename);

    tb_init(&editor_tb, editor_storage, MAX_FILE_CONTENT_LENGTH, initial_content);

    editor_refresh_screen();
}

void editor_refresh_screen() {
    clear_screen();
    char *framebuffer = (char *)VGA_ADDRESS;
    u32 screen_x = 0;
    u32 screen_y = 0;

    u32 target_cursor_screen_x = 0;
    u32 target_cursor_screen_y = 0;

    u32 buffer_len = tb_length(&editor_tb);

    for (u32 i = 0; i <= buffer_len; i++) {
        if (i == tb_cursor_index(&editor_tb)) {
            target_cursor_screen_x = screen_x;
            target_cursor_screen_y = screen_y;
        }

        if (i == buffer_len) {
            break;
        }

        char c = editor_tb.data[i];

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
    if (c == '\b') {
        tb_backspace(&editor_tb);
    } else if (c == '\t') {
        tb_insert_tab(&editor_tb);
    } else {
        tb_insert_char(&editor_tb, c);
    }

    editor_refresh_screen();
}

void editor_exit() {
    current_mode = MODE_NORMAL;
    tb_clear(&editor_tb);
    editor_cursor_x = 0;
    editor_cursor_y = 0;
    editor_filename[0] = '\0';

    // show cursor
    out(0x3D4, 0x0A);
    out(0x3D5, 0x0E);

    clear_screen();
    init_shell_prompt();
}