#ifndef HEAP_H
#define HEAP_H

#include "kernel/kernel.h"

void heap_init();
void* heap_malloc(u32 size);
void heap_free(void* ptr);

#endif
