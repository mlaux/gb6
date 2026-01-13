#ifndef _LRU_H
#define _LRU_H

#include "types.h"

#define BANK0_CACHE_SIZE 0x4000
#define BANKED_CACHE_SIZE 0x4000
#define UPPER_CACHE_SIZE 0x8000
#define MAX_ROM_BANKS 256

// Proactively clear cache when memory is low
#define MIN_FREE_HEAP 65536L

struct code_block *cache_lookup(u16 pc, u8 bank);
void cache_store(u16 pc, u8 bank, struct code_block *block);
void cache_clear_all(void);
void cache_free_bank_arrays(void);
void cache_ensure_memory(void);

// Block caches for different memory regions
extern struct code_block *bank0_cache[BANK0_CACHE_SIZE];
extern struct code_block **banked_cache[MAX_ROM_BANKS];
extern struct code_block *upper_cache[UPPER_CACHE_SIZE];

#endif
