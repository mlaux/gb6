#ifndef _LRU_H
#define _LRU_H

#include "types.h"

// 4096 blocks * sizeof(lru_node) = 64k
#define MAX_CACHED_BLOCKS 4096
#define BANK0_CACHE_SIZE 0x4000
#define BANKED_CACHE_SIZE 0x4000
#define UPPER_CACHE_SIZE 0x8000
#define MAX_ROM_BANKS 256

// Proactively evict blocks when memory is low
#define MIN_FREE_HEAP 65536L

typedef struct lru_node {
    struct code_block *block;
    u16 pc;
    u8 bank;
    u8 in_use;
    struct lru_node *next;
    struct lru_node *prev;
} lru_node;
void lru_init(void);
void lru_clear_all(void);
void lru_ensure_memory(void);
void lru_add_block(struct code_block *block, u16 pc, u8 bank);
void lru_promote(lru_node *node);
struct code_block *cache_lookup(u16 pc, u8 bank);

// Block caches for different memory regions
// Bank 0: 0x0000-0x3FFF (always mapped)
extern struct code_block *bank0_cache[BANK0_CACHE_SIZE];
// Switchable banks: 0x4000-0x7FFF (allocated on demand per bank)
extern struct code_block **banked_cache[MAX_ROM_BANKS];
// Upper region: 0x8000-0xFFFF (WRAM, HRAM, etc.)
extern struct code_block *upper_cache[UPPER_CACHE_SIZE];

#endif