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
    0x06, 0x00, // ld b, $0
    0x0e, 0x11, // ld c, $11
    0x16, 0x22, // ld d, $22
    0x1e, 0x33, // ld e, $33
    0x26, 0x44, // ld h, $44
    0x2e, 0x55, // ld l, $55
    0x36, 0x66, // ld [hl], $66
    0x3e, 0x77, // ld a, $77
    0x01, 0x23, 0x01, // ld bc, $0123
    0x11, 0x67, 0x45, // ld de, $4567
    0x21, 0xab, 0x89, // ld hl, $89ab
    0x31, 0xef, 0xcd, // ld sp, $cdef
    0x3e, 0x0a, // .loop: ld a, 10
    0x3d, // dec a
    0x20, 0xfd, // jr nz, .loop
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