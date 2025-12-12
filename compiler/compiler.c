#include "compiler.h"
#include <stdlib.h>
#include <stdio.h>

// 68k opcodes we'll generate
// moveq #imm, Dn: 0x70 | (Dn << 9) | (imm & 0xff)
//   moveq #xx, d0 = 0x70xx
// rts: 0x4e75

void compiler_init(void)
{
    // nothing for now
}

void emit_byte(struct basic_block *block, uint8_t byte)
{
    if (block->length < sizeof(block->code)) {
        block->code[block->length++] = byte;
    }
}

void emit_word(struct basic_block *block, uint16_t word)
{
    emit_byte(block, word >> 8);
    emit_byte(block, word & 0xff);
}

// Emit: moveq #imm, d0
static void emit_moveq_d0(struct basic_block *block, int8_t imm)
{
    emit_byte(block, 0x70);
    emit_byte(block, (uint8_t)imm);
}

// Emit: rts
static void emit_rts(struct basic_block *block)
{
    emit_word(block, 0x4e75);
}

struct basic_block *compile_block(uint16_t src_address, uint8_t *gb_code)
{
    struct basic_block *block;
    uint16_t src_ptr = 0;
    uint8_t op;
    int done = 0;

    block = malloc(sizeof *block);
    if (!block) {
        return NULL;
    }

    block->length = 0;
    block->src_address = src_address;

    while (!done) {
        op = gb_code[src_ptr++];

        switch (op) {
        case 0x00: // nop
            // emit nothing
            break;

        case 0x3e: // ld a, imm8
            emit_moveq_d0(block, gb_code[src_ptr++]);
            break;

        case 0xc9: // ret
            // End of block - emit rts and stop
            emit_rts(block);
            done = 1;
            break;

        default:
            // Unknown instruction - for now, emit rts and bail
            // In the future, we might emit a call to an interpreter fallback
            fprintf(stderr, "compile_block: unknown opcode 0x%02x at 0x%04x\n",
                    op, src_address + src_ptr - 1);
            emit_rts(block);
            done = 1;
            break;
        }
    }

    block->end_address = src_address + src_ptr;
    return block;
}

void block_free(struct basic_block *block)
{
    free(block);
}
