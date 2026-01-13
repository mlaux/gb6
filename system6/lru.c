#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include <Memory.h>

#include "types.h"
#include "compiler.h"
#include "lru.h"

// Block caches for different memory regions
// Bank 0: 0x0000-0x3FFF (always mapped)
struct code_block *bank0_cache[BANK0_CACHE_SIZE];
// Switchable banks: 0x4000-0x7FFF (allocated on demand per bank)
struct code_block **banked_cache[MAX_ROM_BANKS];
// Upper region: 0x8000-0xFFFF (WRAM, HRAM, etc.)
struct code_block *upper_cache[UPPER_CACHE_SIZE];

// look up cached block for given PC and bank
struct code_block *cache_lookup(u16 pc, u8 bank)
{
    if (pc < 0x4000) {
        return bank0_cache[pc];
    }
    if (pc < 0x8000) {
        if (!banked_cache[bank]) {
            return NULL;
        }
        return banked_cache[bank][pc - 0x4000];
    }
    return upper_cache[pc - 0x8000];
}

// store block in cache for given PC and bank
void cache_store(u16 pc, u8 bank, struct code_block *block)
{
    if (pc < 0x4000) {
        bank0_cache[pc] = block;
    } else if (pc < 0x8000) {
        if (!banked_cache[bank]) {
            // Allocate cache for this bank on demand
            banked_cache[bank] = (struct code_block **)
                calloc(BANKED_CACHE_SIZE, sizeof(struct code_block *));
            if (!banked_cache[bank]) {
                return;  // allocation failed, block won't be cached
            }
        }
        banked_cache[bank][pc - 0x4000] = block;
    } else {
        upper_cache[pc - 0x8000] = block;
    }
}

// Helper: free all blocks in a cache array (but keep array allocated)
static void cache_clear_array(struct code_block **cache, int size)
{
    int k;
    for (k = 0; k < size; k++) {
        if (cache[k]) {
            block_free(cache[k]);
            cache[k] = NULL;
        }
    }
}

// Clear all cached blocks (keeps bank arrays allocated)
void cache_clear_all(void)
{
    int k;
    cache_clear_array(bank0_cache, BANK0_CACHE_SIZE);
    cache_clear_array(upper_cache, UPPER_CACHE_SIZE);
    for (k = 0; k < MAX_ROM_BANKS; k++) {
        if (banked_cache[k]) {
            cache_clear_array(banked_cache[k], BANKED_CACHE_SIZE);
        }
    }
}

// Free bank arrays (for full reset)
void cache_free_bank_arrays(void)
{
    int k;
    for (k = 0; k < MAX_ROM_BANKS; k++) {
        if (banked_cache[k]) {
            free(banked_cache[k]);
            banked_cache[k] = NULL;
        }
    }
}

void cache_ensure_memory(void)
{
    u32 unused;

    if (FreeMem() >= MIN_FREE_HEAP) {
        return;
    }

    cache_clear_all();
    MaxMem(&unused);
}
