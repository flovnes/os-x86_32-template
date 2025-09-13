#include "kernel/kernel.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/timer/timer.h"
#include "drivers/serial_port/serial_port.h"
#include <stdbool.h> // For bool type

// --- Global Variables and Constants ---

// Global variable to keep track of the cursor position
static unsigned short cursor_position = 0;

// Define screen dimensions (common for VGA text mode 80x25)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_FRAMEBUFFER_START 0xB8000

// Color bytes for shell output
#define COLOR_PROMPT_MESSAGE ((0xA << 4) | 0x2) // Light Green (0xA) foreground on Black (0x0) background
#define COLOR_PROMPT_SYMBOL  ((0x2 << 4) | 0xA) // Green (0x2) foreground on Light Green (0xA) background (reversed for effect)
#define COLOR_COMMAND_TEXT   ((0x2 << 4) | 0xF) // White (0xF) foreground on Green (0x2) background
#define COLOR_ERROR_TEXT     ((0x4 << 4) | 0xF) // White (0xF) foreground on Red (0x4) background
#define COLOR_DEFAULT_TEXT   ((0x0 << 4) | 0xF) // White (0xF) foreground on Black (0x0) background

// Shell input buffer
#define SHELL_MAX_INPUT_LENGTH 256
static char shell_input_buffer[SHELL_MAX_INPUT_LENGTH];
static unsigned int shell_input_buffer_index = 0;
static bool command_ready = false; // Flag to indicate if ENTER was pressed

// --- Helper Functions for Screen Management ---

/**
 * Puts cursors in a given position. For example, position = 20 would place it in
 * the first line 20th column, position = 80 will place in the first column of the second line.
 */
void put_cursor(unsigned short pos) {
    out(0x3D4, 14);
    out(0x3D5, ((pos >> 8) & 0x00FF));
    out(0x3D4, 15);
    out(0x3D5, pos & 0x00FF);
}

/**
 * Clears the entire text mode screen by filling it with spaces.
 */
void clear_screen() {
    char *framebuffer = (char *) VGA_FRAMEBUFFER_START;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2) {
        *(framebuffer + i) = ' ';             // Character: space
        *(framebuffer + i + 1) = COLOR_DEFAULT_TEXT; // Default color
    }
}

/**
 * Scrolls the text mode screen up by one line.
 * The top line is removed, and a new blank line appears at the bottom.
 */
void scroll_screen() {
    char *framebuffer = (char *) VGA_FRAMEBUFFER_START;

    // Move all lines up by one
    // Copies (VGA_HEIGHT - 1) lines to the start of the framebuffer
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH * 2; i++) {
        framebuffer[i] = framebuffer[i + VGA_WIDTH * 2];
    }

    // Clear the last line
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH * 2; i < VGA_HEIGHT * VGA_WIDTH * 2; i += 2) {
        *(framebuffer + i) = ' ';             // Character: space
        *(framebuffer + i + 1) = COLOR_DEFAULT_TEXT; // Default color
    }
}

/**
 * Puts a character on the screen at the current cursor_position,
 * handles line wrapping, scrolling, and updates the cursor.
 */
void put_char_on_screen(char c, char color_byte) {
    char *framebuffer = (char *) VGA_FRAMEBUFFER_START;

    if (c == '\n') { // Newline character
        cursor_position = (cursor_position / VGA_WIDTH + 1) * VGA_WIDTH;
    } else if (c == '\b') { // Backspace character
        if (cursor_position > 0) {
            cursor_position--;
            *(framebuffer + cursor_position * 2) = ' '; // Erase character with a space
            *(framebuffer + cursor_position * 2 + 1) = color_byte; // Maintain color
        }
    } else { // Regular printable character
        unsigned short offset = cursor_position * 2;
        *(framebuffer + offset) = c;
        *(framebuffer + offset + 1) = color_byte;
        cursor_position++;
    }

    // Handle line wrap
    if (cursor_position >= VGA_WIDTH * VGA_HEIGHT) {
        scroll_screen();
        // After scrolling, the cursor should be at the beginning of the last line
        cursor_position = (VGA_HEIGHT - 1) * VGA_WIDTH;
    }
    
    put_cursor(cursor_position);
}

void print_string(const char *str, char color_byte) {
    while (*str != '\0') {
        put_char_on_screen(*str, color_byte);
        str++;
    }
}

void print_shell_prompt() {
    print_string("My shell 0.0.1\n", COLOR_PROMPT_MESSAGE);
    print_string("\n$ ", COLOR_PROMPT_SYMBOL);
}

// --- Exception Handler and Kernel Initialization (as in your original code) ---

void exception_handler(u32 interrupt, u32 error, char *message) {
    serial_log(LOG_ERROR, message);
    print_string("KERNEL PANIC: ", COLOR_ERROR_TEXT);
    print_string(message, COLOR_ERROR_TEXT);
    halt_loop(); // Halt if a serious exception occurs
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

/**
 * In order to avoid execution of arbitrary instructions by CPU we halt it.
 * Halt "pauses" CPU and puts it in low power mode until next interrupt occurs.
 */
_Noreturn void halt_loop() {
    while (1) { halt(); }
}

// --- Timer Tick Handler (as in your original code) ---

void timer_tick_handler() {
    // Nothing to do when timer ticks (for now)
}

// --- Keyboard Event Handler ---

void key_handler(struct keyboard_event event) {
    if (event.type == EVENT_KEY_PRESSED) {
        if (event.key_character) {
            if (shell_input_buffer_index < SHELL_MAX_INPUT_LENGTH - 1) { // Leave space for null terminator
                shell_input_buffer[shell_input_buffer_index++] = event.key_character;
                put_char_on_screen(event.key_character, COLOR_COMMAND_TEXT); // Echo to screen
            }
        } else {
            // Handle special keys
            switch (event.key) {
                case KEY_BACKSPACE:
                    if (shell_input_buffer_index > 0) {
                        shell_input_buffer_index--;
                        put_char_on_screen('\b', COLOR_COMMAND_TEXT); // Erase on screen
                    }
                    break;
                case KEY_ENTER:
                    shell_input_buffer[shell_input_buffer_index] = '\0'; // Null-terminate the command
                    put_char_on_screen('\n', COLOR_COMMAND_TEXT); // Move to next line for output
                    command_ready = true; // Signal that a command is ready
                    break;
                case KEY_TAB:
                    // For a shell, Tab usually means autocompletion or inserts spaces.
                    // For now, let's just insert 4 spaces.
                    for (int i = 0; i < 4; ++i) {
                        if (shell_input_buffer_index < SHELL_MAX_INPUT_LENGTH - 1) {
                            shell_input_buffer[shell_input_buffer_index++] = ' ';
                            put_char_on_screen(' ', COLOR_COMMAND_TEXT);
                        }
                    }
                    break;
                default:
                    // Optionally, you might log unhandled key codes for debugging
                    // serial_log(LOG_INFO, "Unhandled key code: %u", event.key_code);
                    break;
            }
        }
    }
}

// --- Shell Command Execution ---

/**
 * Very basic string comparison.
 * In a real kernel, you'd use strncmp to be safer.
 */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}


void execute_command(const char *command) {
    // Trim leading/trailing whitespace if needed (not implemented here for simplicity)
    // Convert to lowercase if commands are case-insensitive (not implemented here)

    if (strcmp(command, "man") == 0) {
        print_string("Command 'man' executed!\n", COLOR_DEFAULT_TEXT);
    } else if (strcmp(command, "z") == 0) {
        print_string("Command 'z' executed!\n", COLOR_DEFAULT_TEXT);
    } else if (strcmp(command, "v") == 0) {
        print_string("Command 'v' executed!\n", COLOR_DEFAULT_TEXT);
    } else if (strcmp(command, "clear") == 0) {
        clear_screen();
        cursor_position = 0; // Reset cursor after clearing
    }
    else {
        print_string("Command not found\n", COLOR_ERROR_TEXT);
    }
    command_ready = false;
}

// --- Main Shell Loop ---

void main_shell_loop() {
    print_shell_prompt(); // Initial prompt

    while (true) {
        if (command_ready) {
            execute_command(shell_input_buffer);
            
            // Reset for next command
            shell_input_buffer_index = 0;
            
            print_shell_prompt(); // Print prompt for next command
        }
        halt(); // Pause CPU until next interrupt (e.g., keyboard or timer)
    }
}


// --- Kernel Entry Point ---

/**
 * This is where the bootloader transfers control to.
 */
void kernel_entry() {
    init_kernel();
    keyboard_set_handler(key_handler);
    timer_set_handler(timer_tick_handler);

    clear_screen();
    cursor_position = 0; // Ensure cursor is at top-left initially

    main_shell_loop(); // Enter the main shell loop
}