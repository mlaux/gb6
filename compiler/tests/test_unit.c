#include "tests.h"

// ==== Unit Tests (check emitted bytes) ====

TEST(test_nop_ret)
{
    uint8_t gb_code[] = { 0x00, 0xc9 };  // nop; ret
    struct basic_block *block = compile_block(0, gb_code);

    // Should just emit rts (nop emits nothing)
    ASSERT_BYTES(block, 0x4e, 0x75);

    block_free(block);
}

TEST(test_ld_a_imm8)
{
    uint8_t gb_code[] = { 0x3e, 0x42, 0xc9 };  // ld a, $42; ret
    struct basic_block *block = compile_block(0, gb_code);

    // moveq #$42, d0 (0x7042) + rts (0x4e75)
    ASSERT_BYTES(block, 0x70, 0x42, 0x4e, 0x75);

    block_free(block);
}

TEST(test_ld_a_zero)
{
    uint8_t gb_code[] = { 0x3e, 0x00, 0xc9 };  // ld a, $00; ret
    struct basic_block *block = compile_block(0, gb_code);

    // moveq #$00, d0 + rts
    ASSERT_BYTES(block, 0x70, 0x00, 0x4e, 0x75);

    block_free(block);
}

TEST(test_ld_a_ff)
{
    uint8_t gb_code[] = { 0x3e, 0xff, 0xc9 };  // ld a, $ff; ret
    struct basic_block *block = compile_block(0, gb_code);

    // moveq #$ff, d0 + rts
    // Note: 0xff is sign-extended to 0xffffffff by moveq, but low byte is correct
    ASSERT_BYTES(block, 0x70, 0xff, 0x4e, 0x75);

    block_free(block);
}

void register_unit_tests(void)
{
    printf("\nUnit tests (byte output):\n");
    RUN_TEST(test_nop_ret);
    RUN_TEST(test_ld_a_imm8);
    RUN_TEST(test_ld_a_zero);
    RUN_TEST(test_ld_a_ff);
}
