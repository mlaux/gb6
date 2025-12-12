#include "tests.h"

// ==== Execution Tests (run on Musashi, check results) ====

TEST(test_exec_ld_a_imm8)
{
    uint8_t gb_code[] = { 0x3e, 0x42, 0xc9 };  // ld a, $42; ret
    struct basic_block *block = compile_block(0, gb_code);

    uint32_t d0 = run_code(block);

    // D0 low byte should be 0x42
    ASSERT_EQ(d0 & 0xff, 0x42);

    block_free(block);
}

TEST(test_exec_ld_a_zero)
{
    uint8_t gb_code[] = { 0x3e, 0x00, 0xc9 };
    struct basic_block *block = compile_block(0, gb_code);

    uint32_t d0 = run_code(block);
    ASSERT_EQ(d0 & 0xff, 0x00);

    block_free(block);
}

TEST(test_exec_ld_a_ff)
{
    uint8_t gb_code[] = { 0x3e, 0xff, 0xc9 };
    struct basic_block *block = compile_block(0, gb_code);

    uint32_t d0 = run_code(block);
    // moveq sign-extends, so D0 = 0xffffffff, but A register is 8-bit
    // The low byte is what matters for the GB A register
    ASSERT_EQ(d0 & 0xff, 0xff);

    block_free(block);
}

TEST(test_exec_multiple_ld_a)
{
    // ld a, $11; ld a, $22; ld a, $33; ret
    uint8_t gb_code[] = { 0x3e, 0x11, 0x3e, 0x22, 0x3e, 0x33, 0xc9 };
    struct basic_block *block = compile_block(0, gb_code);

    uint32_t d0 = run_code(block);
    // Last value should win
    ASSERT_EQ(d0 & 0xff, 0x33);

    block_free(block);
}

void register_exec_tests(void)
{
    printf("\nExecution tests (Musashi):\n");
    RUN_TEST(test_exec_ld_a_imm8);
    RUN_TEST(test_exec_ld_a_zero);
    RUN_TEST(test_exec_ld_a_ff);
    RUN_TEST(test_exec_multiple_ld_a);
}
