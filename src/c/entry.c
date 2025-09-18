#include "kernel/kernel.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/timer/timer.h"
#include "drivers/serial_port/serial_port.h"
#include "drivers/file_system/file_system.h"
#include <stdbool.h> 

#define VGA_ADDRESS 0xb8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define COLORS 0x4 << 4 

static unsigned short current_cursor_pos = 0;
static unsigned short inactivity_counter = 0;
static const u32 SCREENSAVER_TIMEOUT_TICKS = 360;

void scroll_screen(); 
void print_char(char c); 
void print_string(const char *s); 
void clear_screen(); 

#define MAX_COMMAND_LENGTH 256
static char command_buffer[MAX_COMMAND_LENGTH];
static u32 command_buffer_idx = 0;
static bool screensaver_active = false; 

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
        print_string("o help - You're here!\n");
        print_string("  clear\n");
        print_string("  sleep\n");
        print_string("  ls\n");
        print_string("  create <file_name>\n");
        print_string("  write <file_name> <content>\n");
        print_string("  read <file_name>\n");
        print_string("  delete <file_name>\n");
        print_string("  say <text>\n");
    } else if (strcmp(command, "sleep") == 0 || strcmp(command, "gn") == 0) {
        activate_screensaver();
    }
    else if (strcmp(command, "ls") == 0) {
        imfs_list_files();
    } else if (strcmp(command, "create") == 0 || strcmp(command, "touch") == 0) {
        if (arg1 == NULL) {
            print_string("say 'help'\n");
        } else {
            if (imfs_create_file(arg1) == 0) {
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
    } else if (strcmp(command, "read") == 0 || strcmp(command, "cat") == 0) {
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
    print_string("\n$ ");
    for (u32 i = 0; i < command_buffer_idx; i++) {
        print_char(command_buffer[i]);
    }
}

void init_shell() {
    clear_screen();
    print_string("\n >< '> lle fish au chocolat\n");
    init_shell_prompt();
    command_buffer_idx = 0;
}

void key_handler(struct keyboard_event event) {
    if (event.type == EVENT_KEY_PRESSED) {
        inactivity_counter = 0;
        if (screensaver_active) {
            screensaver_active = false;
            clear_screen();
            init_shell_prompt();
            return;
        }
        if (event.key_character >= ' ' && event.key_character <= '~' && command_buffer_idx < MAX_COMMAND_LENGTH - 1) {
            command_buffer[command_buffer_idx++] = event.key_character;
            print_char(event.key_character);
        } else if (event.key == KEY_BACKSPACE) {
            if (command_buffer_idx > 0) {
                command_buffer_idx--;
                print_char('\b');
            }
        }
    } else if (event.type == EVENT_KEY_RELEASED) {
        if (event.key == KEY_ENTER) {
            print_char('\n');
            command_buffer[command_buffer_idx] = '\0';
            execute_command(command_buffer);
            command_buffer_idx = 0;
            init_shell_prompt();
        }
    }
}

void timer_tick_handler() {
    if (screensaver_active) {
        
    } else {
        inactivity_counter++;
        if (inactivity_counter >= SCREENSAVER_TIMEOUT_TICKS) {
            activate_screensaver();
            inactivity_counter = 0;
        }
    }
}

void activate_screensaver() {
    screensaver_active = true;

    // hide the cursor
    out(0x3D4, 0x0A);
    out(0x3D5, 0x20); 

    char *framebuffer = (char *)VGA_ADDRESS;

    clear_screen();
    const char *message = "good night.";
    unsigned short msg_len = 0;
    while(message[msg_len] != '\0') msg_len++;

    unsigned short start_pos = (VGA_WIDTH * (VGA_HEIGHT / 2)) + (VGA_WIDTH / 2) - (msg_len/2+1);
    for (unsigned short i = 0; i < msg_len; i++) {
        framebuffer[(start_pos + i) * 2] = message[i];
    }
}

void kernel_entry() {
    init_kernel();
    keyboard_set_handler(key_handler);
    timer_set_handler(timer_tick_handler);

    init_shell();

    halt_loop();
}

// TODO
// 1. Animate screensaver
// 2. File editor
// 3. Remember the screen before screensaving