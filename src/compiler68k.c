#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

// A -> D0
// BC -> D1
// DE -> D2
// HL -> A0
// SP -> A7

uint8_t out_code[1024];
uint8_t memory[1024]; // ???
uint32_t out_ptr;

struct basic_block {
    // in 68k space?
    uint8_t code[256];
    size_t length;
};

// need some kind of map from gb address to struct basic_block?

uint8_t test_code[] = {
    0x00, // nop
    0x18, 0xfe, // jr $-1
    0xc9  // ret
};

struct basic_block *compile_block(uint16_t src_address, uint8_t *gb_code)
{
    uint8_t instruction;
    struct basic_block *bblock;
    uint32_t dst_ptr = 0;
    uint16_t src_ptr = 0;

    bblock = malloc(sizeof *bblock);
    // bblock->code = out_code + start;

    while (1) {
        instruction = gb_code[src_ptr++];

    }

    return bblock;
}

void run_block(struct basic_block *bblock)
{
    // calling convention? do i need to do this from asm?
    uint16_t jump_target = ((uint16_t (*)()) bblock->code)();
}

// TODO
void block_cache_add(uint16_t src_address, struct basic_block *bblock);
struct basic_block *block_cache_get(uint16_t src_address);

// 1. compile each block ending in a jump
// 2. turn the jump into a return
// 3. add the compiled code to some kind of cache
// 3. return back to check the cache and maybe compile the next block

int main(int argc, char *argv[])
{
    struct basic_block *bblock;
    
    bblock = compile_block(0, test_code);
    block_cache_add(0, bblock);

    while (1) {
        uint16_t jump_target;

        jump_target = ((uint16_t (*)()) bblock->code)();

        bblock = block_cache_get(jump_target);
        if (!bblock) {
            bblock = compile_block(jump_target, test_code + jump_target);
        }
    }
    return 0;
}