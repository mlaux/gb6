#include "tests.h"

TEST_EXEC(test_exec_ld_a_imm8,      REG_A, 0x55,  0x3e, 0x55, 0xc9)
TEST_EXEC(test_exec_ld_a_zero,      REG_A, 0x00,  0x3e, 0x00, 0xc9)
TEST_EXEC(test_exec_ld_a_ff,        REG_A, 0xff,  0x3e, 0xff, 0xc9)
TEST_EXEC(test_exec_multiple_ld_a,  REG_A, 0x33,  0x3e, 0x11, 0x3e, 0x22, 0x3e, 0x33, 0xc9)

TEST_EXEC(test_exec_ld_b_imm8,      REG_BC, 0x00110000,  0x06, 0x11, 0xc9)
TEST_EXEC(test_exec_ld_c_imm8,      REG_BC, 0x00000022,  0x0e, 0x22, 0xc9)
TEST_EXEC(test_exec_ld_d_imm8,      REG_DE, 0x00330000,  0x16, 0x33, 0xc9)
TEST_EXEC(test_exec_ld_e_imm8,      REG_DE, 0x00000044,  0x1e, 0x44, 0xc9)
TEST_EXEC(test_exec_ld_h_imm8,      REG_HL, 0x5500,  0x26, 0x55, 0xc9)
TEST_EXEC(test_exec_ld_l_imm8,      REG_HL, 0x0066,  0x2e, 0x66, 0xc9)

TEST_EXEC(test_exec_ld_bc_imm16,    REG_BC, 0x00110022,  0x01, 0x22, 0x11, 0xc9)
TEST_EXEC(test_exec_ld_de_imm16,    REG_DE, 0x00330044,  0x11, 0x44, 0x33, 0xc9)
TEST_EXEC(test_exec_ld_hl_imm16,    REG_HL, 0x5566,  0x21, 0x66, 0x55, 0xc9)
TEST_EXEC(test_exec_ld_sp_imm16,    REG_SP, 0x7788,  0x31, 0x88, 0x77, 0xc9)

TEST_EXEC(test_dec_a,               REG_A,  0x1,  0x3e, 0x02, 0x3d, 0xc9)

TEST(test_exec_jp_skip)
{
    uint8_t rom[] = {
        0x3e, 0x01,       // 0x0000: ld a, 1
        0xc3, 0x07, 0x00, // 0x0002: jp 0x0007
        0x06, 0x02,       // 0x0005: ld b, 2
        0x0e, 0x03,       // 0x0007: ld c, 3
        0x10              // 0x0009: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(0) & 0xff, 1);           // A = 1
    ASSERT_EQ(get_dreg(1) & 0x00ff00ff, 0x03);  // B = 0, C = 3
}

TEST(test_exec_jr_forward)
{
    // jr with positive displacement skips over instructions
    uint8_t rom[] = {
        0x3e, 0x11,       // 0x0000: ld a, 0x11
        0x18, 0x02,       // 0x0002: jr +2 (skip to 0x0006)
        0x3e, 0x22,       // 0x0004: ld a, 0x22 (skipped)
        0x0e, 0x33,       // 0x0006: ld c, 0x33
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(0) & 0xff, 0x11);        // A should still be 0x11
    ASSERT_EQ(get_dreg(1) & 0xff, 0x33);        // C = 0x33
}

TEST(test_exec_jr_zero)
{
    // jr with displacement 0 is effectively a no-op (jumps to next instruction)
    uint8_t rom[] = {
        0x3e, 0xaa,       // 0x0000: ld a, 0xaa
        0x18, 0x00,       // 0x0002: jr +0 (jump to 0x0004)
        0x0e, 0xbb,       // 0x0004: ld c, 0xbb
        0x10              // 0x0006: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(0) & 0xff, 0xaa);
    ASSERT_EQ(get_dreg(1) & 0xff, 0xbb);
}

void register_exec_tests(void)
{
    printf("\nExecution tests:\n");
    RUN_TEST(test_exec_ld_a_imm8);
    RUN_TEST(test_exec_ld_a_zero);
    RUN_TEST(test_exec_ld_a_ff);
    RUN_TEST(test_exec_multiple_ld_a);

    RUN_TEST(test_exec_ld_b_imm8);
    RUN_TEST(test_exec_ld_c_imm8);
    RUN_TEST(test_exec_ld_d_imm8);
    RUN_TEST(test_exec_ld_e_imm8);
    RUN_TEST(test_exec_ld_h_imm8);
    RUN_TEST(test_exec_ld_l_imm8);

    RUN_TEST(test_exec_ld_bc_imm16);
    RUN_TEST(test_exec_ld_de_imm16);
    RUN_TEST(test_exec_ld_hl_imm16);
    RUN_TEST(test_exec_ld_sp_imm16);

    RUN_TEST(test_dec_a);

    printf("\nMulti-block tests:\n");
    RUN_TEST(test_exec_jp_skip);
    RUN_TEST(test_exec_jr_forward);
    RUN_TEST(test_exec_jr_zero);
}
