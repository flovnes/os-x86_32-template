#ifndef COMMANDS_H
#define COMMANDS_H

#include "kernel/kernel.h"

void execute_command(const char *command_line, bool *mode_changed_out);

#endif
