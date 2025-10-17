#include "heap.h"
#include <stddef.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#define HEAP_INITIAL_SIZE 100
#define HEAP_MAX_SIZE 1000
#define HEAP_EXPAND_SIZE 100

struct heap_block {
	bool in_use;
	u32 size;
};

static char heap[HEAP_MAX_SIZE];
static u32 heap_size = HEAP_INITIAL_SIZE;
static u32 heap_used = 0;

void heap_init() {
	heap_size = HEAP_INITIAL_SIZE;
	heap_used = 0;
}

void* heap_malloc(u32 size) {
	u32 total_size = size + sizeof(struct heap_block);
	if (heap_used + total_size > heap_size) {
		u32 needed = heap_used + total_size - heap_size;
		u32 expand_by = ((needed + HEAP_EXPAND_SIZE - 1) / HEAP_EXPAND_SIZE) * HEAP_EXPAND_SIZE;
		if (heap_size + expand_by > HEAP_MAX_SIZE) {
			return NULL;
		}
		heap_size += expand_by;
	}

	struct heap_block* best_block = NULL;
	u32 best_size = 0;
	u32 offset = 0;
	while (offset < heap_used) {
		struct heap_block* block = (struct heap_block*)(heap + offset);
		if (!block->in_use && block->size >= total_size && block->size > best_size) {
			best_block = block;
			best_size = block->size;
		}
		offset += sizeof(struct heap_block) + block->size;
	}

	if (!best_block) {
		if (heap_used + total_size > heap_size) {
			return NULL;
		}
		best_block = (struct heap_block*)(heap + heap_used);
		best_block->in_use = true;
		best_block->size = total_size;
		heap_used += total_size;
		return (char*)best_block + sizeof(struct heap_block);
	}

	best_block->in_use = true;
	best_block->size = total_size;
	return (char*)best_block + sizeof(struct heap_block);
}

void heap_free(void* ptr) {
	if (!ptr) return;
	struct heap_block* block = (struct heap_block*)((char*)ptr - sizeof(struct heap_block));
	block->in_use = false;
	u32 offset = 0;
	u32 last_used_offset = 0;
	while (offset < heap_used) {
		struct heap_block* current_block = (struct heap_block*)(heap + offset);
		if (current_block->in_use) {
			last_used_offset = offset + sizeof(struct heap_block) + current_block->size;
		}
		offset += sizeof(struct heap_block) + current_block->size;
	}
	heap_used = last_used_offset;
}
