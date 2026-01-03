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

static lru_node lru_pool[MAX_CACHED_BLOCKS];
static lru_node *lru_head;  // most recently used
static lru_node *lru_tail;  // least recently used
static lru_node *lru_free;  // free list head
static int lru_count;

// look up cached block for given PC and bank
// basically the same as what the asm dispatcher does, but in C
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
static void cache_store(u16 pc, u8 bank, struct code_block *block)
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

static void lru_clear_cache_entry(lru_node *node)
{
    u16 pc = node->pc;
    u8 bank = node->bank;

    if (pc < 0x4000) {
        bank0_cache[pc] = NULL;
    } else if (pc < 0x8000) {
        if (banked_cache[bank]) {
            banked_cache[bank][pc - 0x4000] = NULL;
        }
    } else {
        upper_cache[pc - 0x8000] = NULL;
    }
}

// Helper: free all blocks in a cache array
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

static void clear_block_caches(void)
{
  int k;
  cache_clear_array(bank0_cache, BANK0_CACHE_SIZE);
  cache_clear_array(upper_cache, UPPER_CACHE_SIZE);
  for (k = 0; k < MAX_ROM_BANKS; k++) {
    if (banked_cache[k]) {
      cache_clear_array(banked_cache[k], BANKED_CACHE_SIZE);
      free(banked_cache[k]);
      banked_cache[k] = NULL;
    }
  }
}

// Initialize LRU system
void lru_init(void)
{
    int k;

    lru_head = NULL;
    lru_tail = NULL;
    lru_count = 0;

    // Build free list
    lru_free = &lru_pool[0];
    for (k = 0; k < MAX_CACHED_BLOCKS - 1; k++) {
        lru_pool[k].next = &lru_pool[k + 1];
        lru_pool[k].in_use = 0;
        lru_pool[k].block = NULL;
    }
    lru_pool[MAX_CACHED_BLOCKS - 1].next = NULL;
    lru_pool[MAX_CACHED_BLOCKS - 1].in_use = 0;
    lru_pool[MAX_CACHED_BLOCKS - 1].block = NULL;

    clear_block_caches();
}

// Remove node from LRU list (but don't free it)
static void lru_unlink(lru_node *node)
{
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        lru_head = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        lru_tail = node->prev;
    }
    node->prev = NULL;
    node->next = NULL;
}

// Add node to front of LRU list (most recent)
static void lru_push_front(lru_node *node)
{
    node->prev = NULL;
    node->next = lru_head;
    if (lru_head) {
        lru_head->prev = node;
    } else {
        lru_tail = node;
    }
    lru_head = node;
}

// Promote node to front (on cache hit)
void lru_promote(lru_node *node)
{
    if (node == lru_head) {
        return;
    }
    lru_unlink(node);
    lru_push_front(node);
}

// Evict least recently used block
static void lru_evict_one(void)
{
    lru_node *victim;

    if (!lru_tail) {
        return;
    }

    victim = lru_tail;
    lru_unlink(victim);

    // Clear cache entry
    lru_clear_cache_entry(victim);

    // Free the block
    if (victim->block) {
        block_free(victim->block);
        victim->block = NULL;
    }

    // Return to free list
    victim->in_use = 0;
    victim->next = lru_free;
    lru_free = victim;
    lru_count--;
}

// Get a free node, evicting if necessary
static lru_node *lru_alloc_node(void)
{
    lru_node *node;

    if (!lru_free) {
        lru_evict_one();
    }

    if (!lru_free) {
        return NULL;  // shouldn't happen
    }

    node = lru_free;
    lru_free = node->next;
    node->next = NULL;
    node->prev = NULL;
    node->in_use = 1;
    node->block = NULL;
    lru_count++;

    return node;
}

// Add a new block to LRU and cache
void lru_add_block(struct code_block *block, u16 pc, u8 bank)
{
    lru_node *node = lru_alloc_node();

    if (!node) {
        return;
    }

    node->block = block;
    node->pc = pc;
    node->bank = bank;

    // Store back-pointer for promotion on hit
    block->lru_node = node;

    lru_push_front(node);

    // Store in cache
    cache_store(pc, bank, block);
}

// Clear all LRU entries (for reset)
void lru_clear_all(void)
{
    lru_node *node;
    int k;

    // Free all blocks in use
    for (k = 0; k < MAX_CACHED_BLOCKS; k++) {
        node = &lru_pool[k];
        if (node->in_use && node->block) {
            lru_clear_cache_entry(node);
            block_free(node->block);
            node->block = NULL;
        }
    }

    // Reinitialize
    lru_init();
}

void lru_ensure_memory(void)
{
    while (FreeMem() < MIN_FREE_HEAP && lru_count > 0) {
        lru_evict_one();
    }
}
