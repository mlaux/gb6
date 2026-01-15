#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "types.h"
#include "cache.h"
#include "arena.h"

static void **bank0_cache;
static void **upper_cache;
static void ***banked_cache;

// Look up cached code pointer for given PC and bank
void *cache_lookup(u16 pc, u8 bank)
{
    if (pc < 0x4000) {
        if (!bank0_cache) {
            return NULL;
        }
        return bank0_cache[pc];
    }
    if (pc < 0x8000) {
        if (!banked_cache || !banked_cache[bank]) {
            return NULL;
        }
        return banked_cache[bank][pc - 0x4000];
    }
    if (!upper_cache) {
        return NULL;
    }
    return upper_cache[pc - 0x8000];
}

// Store code pointer in cache for given PC and bank
void cache_store(u16 pc, u8 bank, void *code)
{
    if (pc < 0x4000) {
        bank0_cache[pc] = code;
    } else if (pc < 0x8000) {
        if (!banked_cache[bank]) {
            banked_cache[bank] = arena_alloc(BANKED_CACHE_SIZE * sizeof(void *));
            if (!banked_cache[bank]) {
                return;
            }
            memset(banked_cache[bank], 0, BANKED_CACHE_SIZE * sizeof(void *));
        }
        banked_cache[bank][pc - 0x4000] = code;
    } else {
        upper_cache[pc - 0x8000] = code;
    }
}

// Allocate and zero all cache arrays upfront
// Returns 1 on success, 0 on failure
int cache_init(void)
{
    bank0_cache = arena_alloc(BANK0_CACHE_SIZE * sizeof(void *));
    if (!bank0_cache) {
        return 0;
    }
    memset(bank0_cache, 0, BANK0_CACHE_SIZE * sizeof(void *));

    upper_cache = arena_alloc(UPPER_CACHE_SIZE * sizeof(void *));
    if (!upper_cache) {
        return 0;
    }
    memset(upper_cache, 0, UPPER_CACHE_SIZE * sizeof(void *));

    // Just the array of bank pointers, not each bank's cache
    banked_cache = arena_alloc(MAX_ROM_BANKS * sizeof(void **));
    if (!banked_cache) {
        return 0;
    }
    memset(banked_cache, 0, MAX_ROM_BANKS * sizeof(void **));

    return 1;
}

// Get current cache array pointers for dispatcher
void cache_get_arrays(void ***out_bank0, void ****out_banked, void ***out_upper)
{
    *out_bank0 = bank0_cache;
    *out_banked = banked_cache;
    *out_upper = upper_cache;
}
