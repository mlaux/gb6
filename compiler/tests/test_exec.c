#include "tests.h"

/*
  Instruction roadmap:
  1. ld b,imm8 / ld c,imm8 etc (other 8-bit immediate loads)
  2. ld bc,imm16 / ld de,imm16 / ld hl,imm16 (16-bit loads)
  3. Simple ALU: inc a, dec a, add a,imm8
  4. Register-to-register moves: ld a,b, ld b,c, etc.
  5. Memory access: ld a,[hl], ld [hl],a
  6. Control flow: jr, jp, conditional jumps
*/

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
}
