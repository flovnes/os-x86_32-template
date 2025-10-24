#include "commands.h"
#include "string.h"
#include "file_system.h"
#include "screensaver.h"
#include "heap.h"

extern void clear_screen();
extern void print_string(const char *s);
extern void print_char(char c);
extern void editor_init(const char *filename, const char *initial_content);
extern const char* read_file(const char *filename);
extern int write_file(const char *filename, const char *content);
extern int create_file(const char *filename);
extern int delete_file(const char *filename);
extern void fs_list_files();

void print_u32_dec(u32 value) {
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

static void print_hex(u32 value) {
	const char hex_chars[] = "0123456789ABCDEF";
	if (value == 0) { print_char('0'); return; }
	char buf[9];
	int i = 0;
	while (value > 0) { buf[i++] = hex_chars[value & 0xF]; value >>= 4; }
	for (int j = i - 1; j >= 0; j--) { print_char(buf[j]); }
}

static void cmd_clear(const char *a1, const char *a2) { (void)a1; (void)a2; clear_screen(); }
static void cmd_help(const char *a1, const char *a2) {
	(void)a1; (void)a2;
	print_string("   list\n");
	print_string("   clear\n");
	print_string("   sleep\n");
	print_string(" o help - You're here!\n");
	print_string("   create <file_name>\n");
	print_string("   write <file_name> <content>\n");
	print_string("   edit <file_name>\n");
	print_string("   read <file_name>\n");
	print_string("   delete <file_name>\n");
	print_string("   say <text>\n");
	print_string("   malloc <size>\n");
	print_string("   free <ptr>\n");
	print_string("   mlist\n");
}
static void cmd_sleep(const char *a1, const char *a2) { (void)a1; (void)a2; activate_screensaver(); }
static void cmd_malloc(const char *a1, const char *a2) {
	(void)a2;
	u32 size = 0;
	if (!parse_u32_dec(a1, &size)) { print_string("Usage: malloc <size>\n"); return; }
	void* ptr = heap_malloc(size);
	if (ptr) {
		print_string("ptr=");
		print_u32_dec((u32)ptr);
		print_string(" size=");
		print_u32_dec(size);
		print_char('\n');
	} else {
		print_string("Out of memory\n");
	}
}
static void cmd_free(const char *a1, const char *a2) {
	(void)a2;
	u32 ptr_val = 0;
	if (!parse_u32_dec(a1, &ptr_val)) { print_string("Usage: free <ptr>\n"); return; }
	heap_free((void*)ptr_val);
	print_string("Freed\n");
}
static void cmd_memlist(const char *a1, const char *a2) { (void)a1; (void)a2; heap_list_allocated(); }
static void cmd_ls(const char *a1, const char *a2) { (void)a1; (void)a2; fs_list_files(); }
static void cmd_create(const char *a1, const char *a2) {
	(void)a2;
	if (a1 == NULL) { print_string("say 'help'\n"); return; }
	if (create_file(a1) == 0) { print_string("File '"); print_string(a1); print_string("' created.\n"); }
	else { print_string("Failed to create file '"); print_string(a1); print_string("'.\n"); }
}
static void cmd_write(const char *a1, const char *a2) {
	if (a1 == NULL || a2 == NULL) { print_string("say 'help'\n"); return; }
	if (write_file(a1, a2) == 0) { print_string("Content written to '"); print_string(a1); print_string("'.\n"); }
	else { print_string("Failed to write to file '"); print_string(a1); print_string("'.\n"); }
}
static void cmd_edit(const char *a1, const char *a2) {
	(void)a2;
	if (a1 == NULL) { print_string("say 'help'\n"); return; }
	if (strlen_custom(a1) > 32) { print_string("Error: filename > 32 chars).\n"); return; }
	const char *initial_content = read_file(a1);
	if (initial_content == NULL) { initial_content = "\n"; }
	editor_init(a1, initial_content);
}
static void cmd_read(const char *a1, const char *a2) {
	(void)a2;
	if (a1 == NULL) { print_string("say 'help'\n"); return; }
	const char *content = read_file(a1);
	if (content != NULL) { print_string(content); print_char('\n'); }
	else { print_string("File '"); print_string(a1); print_string("' not found.\n"); }
}
static void cmd_delete(const char *a1, const char *a2) {
	(void)a2;
	if (a1 == NULL) { print_string("say 'help'\n"); return; }
	if (delete_file(a1) == 0) { print_string("File '"); print_string(a1); print_string("' deleted.\n"); }
	else { print_string("Failed to delete file '"); print_string(a1); print_string("'.\n"); }
}
static void cmd_say(const char *a1, const char *a2) { (void)a2; if (a1) print_string(a1); print_char('\n'); }

struct command_entry { const char *name; void (*handler)(const char*, const char*); };
static const struct command_entry COMMANDS[] = {
	{ "clear",  cmd_clear },
	{ "cls",    cmd_clear },
	{ "help",   cmd_help },
	{ "fish",   cmd_help },
	{ "sleep",  cmd_sleep },
	{ "gn",     cmd_sleep },
	{ "malloc", cmd_malloc },
	{ "free",   cmd_free },
	{ "mlist", cmd_memlist },
	{ "mem", cmd_memlist },
	{ "ls",     cmd_ls },
	{ "list",     cmd_ls },
	{ "create", cmd_create },
	{ "touch",  cmd_create },
	{ "new",  cmd_create },
	{ "write",  cmd_write },
	{ "edit",   cmd_edit },
	{ "v",      cmd_edit },
	{ "read",   cmd_read },
	{ "cat",    cmd_read },
	{ "delete", cmd_delete },
	{ "rm",     cmd_delete },
	{ "say",    cmd_say },
	{ "echo",   cmd_say },
};
static const u32 COMMAND_COUNT = sizeof(COMMANDS)/sizeof(COMMANDS[0]);

static const char* skip_ws(const char *s) { while (s && *s==' ') s++; return s; }
static const char* next_token(const char *s, char *out, u32 out_cap) {
	if (!s) return NULL;
	s = skip_ws(s);
	u32 i = 0;
	while (*s && *s!=' ') { if (i+1<out_cap) out[i++] = *s; s++; }
	out[i] = '\0';
	return skip_ws(s);
}

void execute_command(const char *command_line) {
	char cmd[64];
	char arg1[256];
	char arg2[256];
	const char *p = command_line;
	p = next_token(p, cmd, sizeof(cmd));
	p = next_token(p, arg1, sizeof(arg1));
	if (p) {
		u32 i = 0; while (*p && i+1<sizeof(arg2)) { arg2[i++] = *p++; } arg2[i] = '\0';
	} else { arg2[0] = '\0'; }

	if (cmd[0] == '\0') return;
	for (u32 i = 0; i < COMMAND_COUNT; i++) {
		if (strcmp(COMMANDS[i].name, cmd) == 0) { COMMANDS[i].handler(arg1[0]?arg1:NULL, arg2[0]?arg2:NULL); return; }
	}
	print_string("\n  ? Unknown command\n");
}
