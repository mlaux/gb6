#include "tests.h"

// SWAP tests
// ld a, $12; swap a -> A = 0x21
TEST_EXEC(test_swap_a,              REG_A,  0x21, 0x3e, 0x12, 0xcb, 0x37, 0x10)
// ld b, $ab; swap b -> B = 0xba
TEST_EXEC(test_swap_b,              REG_BC, 0x00ba0000, 0x06, 0xab, 0xcb, 0x30, 0x10)
// ld c, $12; swap c -> C = 0x21
TEST_EXEC(test_swap_c,              REG_BC, 0x00000021, 0x0e, 0x12, 0xcb, 0x31, 0x10)
// ld d, $f0; swap d -> D = 0x0f
TEST_EXEC(test_swap_d,              REG_DE, 0x000f0000, 0x16, 0xf0, 0xcb, 0x32, 0x10)
// ld e, $34; swap e -> E = 0x43
TEST_EXEC(test_swap_e,              REG_DE, 0x00000043, 0x1e, 0x34, 0xcb, 0x33, 0x10)
// ld hl, $1234; swap h -> H = 0x21, HL = 0x2134
TEST_EXEC(test_swap_h,              REG_HL, 0x2134, 0x21, 0x34, 0x12, 0xcb, 0x34, 0x10)
// ld hl, $1234; swap l -> L = 0x43, HL = 0x1243
TEST_EXEC(test_swap_l,              REG_HL, 0x1243, 0x21, 0x34, 0x12, 0xcb, 0x35, 0x10)

// RLC tests - rotate left circular (bit 7 -> C and bit 0)
// ld a, $80; rlc a -> A = 0x01, C = 1
TEST_EXEC(test_rlc_a,               REG_A,  0x01, 0x3e, 0x80, 0xcb, 0x07, 0x10)
// ld a, $81; rlc a -> A = 0x03, C = 1
TEST_EXEC(test_rlc_a_both_bits,     REG_A,  0x03, 0x3e, 0x81, 0xcb, 0x07, 0x10)
// ld b, $40; rlc b -> B = 0x80, C = 0
TEST_EXEC(test_rlc_b,               REG_BC, 0x00800000, 0x06, 0x40, 0xcb, 0x00, 0x10)
// ld c, $01; rlc c -> C = 0x02
TEST_EXEC(test_rlc_c,               REG_BC, 0x00000002, 0x0e, 0x01, 0xcb, 0x01, 0x10)
// ld hl, $8080; rlc h -> H = 0x01 (bit 7 wraps to bit 0), HL = 0x0180
TEST_EXEC(test_rlc_h,               REG_HL, 0x0180, 0x21, 0x80, 0x80, 0xcb, 0x04, 0x10)
// ld hl, $8001; rlc l -> L = 0x02, HL = 0x8002
TEST_EXEC(test_rlc_l,               REG_HL, 0x8002, 0x21, 0x01, 0x80, 0xcb, 0x05, 0x10)

// RRC tests - rotate right circular (bit 0 -> C and bit 7)
// ld a, $01; rrc a -> A = 0x80, C = 1
TEST_EXEC(test_rrc_a,               REG_A,  0x80, 0x3e, 0x01, 0xcb, 0x0f, 0x10)
// ld a, $81; rrc a -> A = 0xc0, C = 1
TEST_EXEC(test_rrc_a_both_bits,     REG_A,  0xc0, 0x3e, 0x81, 0xcb, 0x0f, 0x10)
// ld d, $02; rrc d -> D = 0x01, C = 0
TEST_EXEC(test_rrc_d,               REG_DE, 0x00010000, 0x16, 0x02, 0xcb, 0x0a, 0x10)
// ld e, $80; rrc e -> E = 0x40
TEST_EXEC(test_rrc_e,               REG_DE, 0x00000040, 0x1e, 0x80, 0xcb, 0x0b, 0x10)

// SLA tests - shift left arithmetic (bit 7 -> C, 0 -> bit 0)
// ld a, $40; sla a -> A = 0x80, C = 0
TEST_EXEC(test_sla_a,               REG_A,  0x80, 0x3e, 0x40, 0xcb, 0x27, 0x10)
// ld a, $81; sla a -> A = 0x02, C = 1
TEST_EXEC(test_sla_a_carry,         REG_A,  0x02, 0x3e, 0x81, 0xcb, 0x27, 0x10)
// ld b, $01; sla b -> B = 0x02
TEST_EXEC(test_sla_b,               REG_BC, 0x00020000, 0x06, 0x01, 0xcb, 0x20, 0x10)

// SRA tests - shift right arithmetic (bit 7 preserved, bit 0 -> C)
// ld a, $82; sra a -> A = 0xc1, C = 0 (sign bit preserved)
TEST_EXEC(test_sra_a,               REG_A,  0xc1, 0x3e, 0x82, 0xcb, 0x2f, 0x10)
// ld a, $81; sra a -> A = 0xc0, C = 1
TEST_EXEC(test_sra_a_carry,         REG_A,  0xc0, 0x3e, 0x81, 0xcb, 0x2f, 0x10)
// ld a, $02; sra a -> A = 0x01 (positive number)
TEST_EXEC(test_sra_a_positive,      REG_A,  0x01, 0x3e, 0x02, 0xcb, 0x2f, 0x10)
// ld c, $04; sra c -> C = 0x02
TEST_EXEC(test_sra_c,               REG_BC, 0x00000002, 0x0e, 0x04, 0xcb, 0x29, 0x10)

// SRL tests - shift right logical (0 -> bit 7, bit 0 -> C)
// ld a, $82; srl a -> A = 0x41, C = 0
TEST_EXEC(test_srl_a,               REG_A,  0x41, 0x3e, 0x82, 0xcb, 0x3f, 0x10)
// ld a, $81; srl a -> A = 0x40, C = 1
TEST_EXEC(test_srl_a_carry,         REG_A,  0x40, 0x3e, 0x81, 0xcb, 0x3f, 0x10)
// ld d, $08; srl d -> D = 0x04
TEST_EXEC(test_srl_d,               REG_DE, 0x00040000, 0x16, 0x08, 0xcb, 0x3a, 0x10)
// ld hl, $0202; srl h -> H = 0x01, HL = 0x0102
TEST_EXEC(test_srl_h,               REG_HL, 0x0102, 0x21, 0x02, 0x02, 0xcb, 0x3c, 0x10)
// ld hl, $0104; srl l -> L = 0x02, HL = 0x0102
TEST_EXEC(test_srl_l,               REG_HL, 0x0102, 0x21, 0x04, 0x01, 0xcb, 0x3d, 0x10)

// RL tests - rotate left through carry (old C -> bit 0, bit 7 -> new C)
TEST(test_rl_a_carry_in)
{
    // scf (or equivalent: ld a,$ff; cp a,$00 to set C), then rl a
    // Set C via: ld a,$00; cp a,$01 -> A<1, so C=1
    uint8_t rom[] = {
        0x3e, 0x00,       // ld a, 0x00
        0xfe, 0x01,       // cp a, 0x01 -> sets C=1 (0 < 1)
        0x3e, 0x00,       // ld a, 0x00
        0xcb, 0x17,       // rl a -> old C (1) goes to bit 0, so A = 0x01
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x01);
}

TEST(test_rl_a_carry_out)
{
    // rl a with bit 7 set should set carry
    uint8_t rom[] = {
        0xaf,             // xor a (clears carry, A=0)
        0x3e, 0x80,       // ld a, 0x80
        0xcb, 0x17,       // rl a -> bit 7 goes to C, A = 0x00
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x00);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 1, 1);  // C flag set
}

// RR tests - rotate right through carry (old C -> bit 7, bit 0 -> new C)
TEST(test_rr_a_carry_in)
{
    // Set C=1, then rr a with 0 -> result should be 0x80
    uint8_t rom[] = {
        0x3e, 0x00,       // ld a, 0x00
        0xfe, 0x01,       // cp a, 0x01 -> sets C=1
        0x3e, 0x00,       // ld a, 0x00
        0xcb, 0x1f,       // rr a -> old C (1) goes to bit 7, so A = 0x80
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x80);
}

TEST(test_rr_a_carry_out)
{
    // rr a with bit 0 set should set carry
    uint8_t rom[] = {
        0xaf,             // xor a (clears carry, A=0)
        0x3e, 0x01,       // ld a, 0x01
        0xcb, 0x1f,       // rr a -> bit 0 goes to C, A = 0x00
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x00);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 1, 1);  // C flag set
}

// (HL) memory tests
TEST(test_rlc_hl_ind)
{
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // ld hl, 0x5000
        0x36, 0x81,       // ld (hl), 0x81
        0xcb, 0x06,       // rlc (hl)
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_mem_byte(0x5000), 0x03);  // 0x81 rotated left = 0x03
}

TEST(test_srl_hl_ind)
{
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // ld hl, 0x5000
        0x36, 0x82,       // ld (hl), 0x82
        0xcb, 0x3e,       // srl (hl)
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_mem_byte(0x5000), 0x41);  // 0x82 >> 1 = 0x41
}

TEST(test_swap_hl_ind)
{
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // ld hl, 0x5000
        0x36, 0xab,       // ld (hl), 0xab
        0xcb, 0x36,       // swap (hl)
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_mem_byte(0x5000), 0xba);  // 0xab swapped = 0xba
}

// Flag tests for shifts/rotates
TEST(test_rlc_zero_flag)
{
    // rlc of 0x00 should set Z flag
    uint8_t rom[] = {
        0x3e, 0x00,       // ld a, 0x00
        0xcb, 0x07,       // rlc a
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x00);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x04, 0x04);  // Z flag set
}

TEST(test_sla_carry_flag)
{
    // sla of 0x80 should set C flag
    uint8_t rom[] = {
        0x3e, 0x80,       // ld a, 0x80
        0xcb, 0x27,       // sla a
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x00);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 1, 1);  // C flag set
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x04, 0x04);  // Z flag set
}

TEST(test_swap_clears_carry)
{
    // swap should clear C flag
    uint8_t rom[] = {
        0x3e, 0x00,       // ld a, 0x00
        0xfe, 0x01,       // cp a, 0x01 -> sets C=1
        0x3e, 0x12,       // ld a, 0x12
        0xcb, 0x37,       // swap a (clears C)
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x21);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x01, 0x00);  // C flag clear
}

void register_cb_tests(void)
{
    printf("\nSWAP tests:\n");
    RUN_TEST(test_swap_a);
    RUN_TEST(test_swap_b);
    RUN_TEST(test_swap_c);
    RUN_TEST(test_swap_d);
    RUN_TEST(test_swap_e);
    RUN_TEST(test_swap_h);
    RUN_TEST(test_swap_l);

    printf("\nRLC tests:\n");
    RUN_TEST(test_rlc_a);
    RUN_TEST(test_rlc_a_both_bits);
    RUN_TEST(test_rlc_b);
    RUN_TEST(test_rlc_c);
    RUN_TEST(test_rlc_h);
    RUN_TEST(test_rlc_l);

    printf("\nRRC tests:\n");
    RUN_TEST(test_rrc_a);
    RUN_TEST(test_rrc_a_both_bits);
    RUN_TEST(test_rrc_d);
    RUN_TEST(test_rrc_e);

    printf("\nSLA tests:\n");
    RUN_TEST(test_sla_a);
    RUN_TEST(test_sla_a_carry);
    RUN_TEST(test_sla_b);

    printf("\nSRA tests:\n");
    RUN_TEST(test_sra_a);
    RUN_TEST(test_sra_a_carry);
    RUN_TEST(test_sra_a_positive);
    RUN_TEST(test_sra_c);

    printf("\nSRL tests:\n");
    RUN_TEST(test_srl_a);
    RUN_TEST(test_srl_a_carry);
    RUN_TEST(test_srl_d);
    RUN_TEST(test_srl_h);
    RUN_TEST(test_srl_l);

    printf("\nRL/RR tests:\n");
    RUN_TEST(test_rl_a_carry_in);
    RUN_TEST(test_rl_a_carry_out);
    RUN_TEST(test_rr_a_carry_in);
    RUN_TEST(test_rr_a_carry_out);

    printf("\n(HL) indirect tests:\n");
    RUN_TEST(test_rlc_hl_ind);
    RUN_TEST(test_srl_hl_ind);
    RUN_TEST(test_swap_hl_ind);

    printf("\nCB prefix flag tests:\n");
    RUN_TEST(test_rlc_zero_flag);
    RUN_TEST(test_sla_carry_flag);
    RUN_TEST(test_swap_clears_carry);
}
