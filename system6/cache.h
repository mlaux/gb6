#ifndef _LRU_H
#define _LRU_H

#include "types.h"

#define BANK0_CACHE_SIZE 0x4000
#define BANKED_CACHE_SIZE 0x4000
#define UPPER_CACHE_SIZE 0x8000
#define MAX_ROM_BANKS 256

// Allocate and zero all cache arrays upfront (call after arena init/reset)
int cache_init(void);

// Returns cached code pointer or NULL
void *cache_lookup(u16 pc, u8 bank);

// Store code pointer in cache
void cache_store(u16 pc, u8 bank, void *code);

// Get current cache array pointers for dispatcher
// this is the first time i've ever used a ****
void cache_get_arrays(void ***out_bank0, void ****out_banked, void ***out_upper);

#endif
