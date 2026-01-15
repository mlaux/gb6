#ifndef _ARENA_H
#define _ARENA_H

#include <stddef.h>

// initialize arena by allocating the largest available contiguous block
// minus a safety margin, returns 1 on success and 0 on failure
int arena_init(void);

// bump-allocate from the arena, returns NULL if no space
void *arena_alloc(size_t size);

// reset arena pointer to base for instant "free all"
void arena_reset(void);

// return bytes remaining in arena
size_t arena_remaining(void);

void arena_destroy(void);

#endif
