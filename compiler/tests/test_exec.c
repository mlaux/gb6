#include "tests.h"

TEST_EXEC(test_exec_ld_a_imm8,      REG_A, 0x55,  0x3e, 0x55, 0x10)
TEST_EXEC(test_exec_ld_a_zero,      REG_A, 0x00,  0x3e, 0x00, 0x10)
TEST_EXEC(test_exec_ld_a_ff,        REG_A, 0xff,  0x3e, 0xff, 0x10)
TEST_EXEC(test_exec_multiple_ld_a,  REG_A, 0x33,  0x3e, 0x11, 0x3e, 0x22, 0x3e, 0x33, 0x10)

TEST_EXEC(test_exec_ld_b_imm8,      REG_BC, 0x00110000,  0x06, 0x11, 0x10)
TEST_EXEC(test_exec_ld_c_imm8,      REG_BC, 0x00000022,  0x0e, 0x22, 0x10)
TEST_EXEC(test_exec_ld_d_imm8,      REG_DE, 0x00330000,  0x16, 0x33, 0x10)
TEST_EXEC(test_exec_ld_e_imm8,      REG_DE, 0x00000044,  0x1e, 0x44, 0x10)
TEST_EXEC(test_exec_ld_h_imm8,      REG_HL, 0x5500,  0x26, 0x55, 0x10)
TEST_EXEC(test_exec_ld_l_imm8,      REG_HL, 0x0066,  0x2e, 0x66, 0x10)

TEST_EXEC(test_exec_ld_bc_imm16,    REG_BC, 0x00110022,  0x01, 0x22, 0x11, 0x10)
TEST_EXEC(test_exec_ld_de_imm16,    REG_DE, 0x00330044,  0x11, 0x44, 0x33, 0x10)
TEST_EXEC(test_exec_ld_hl_imm16,    REG_HL, 0x5566,  0x21, 0x66, 0x55, 0x10)
TEST_EXEC(test_exec_ld_sp_imm16,    REG_SP, 0x7788,  0x31, 0x88, 0x77, 0x10)

TEST_EXEC(test_dec_a,               REG_A,  0x1,  0x3e, 0x02, 0x3d, 0x10)
TEST_EXEC(test_dec_b,               REG_BC, 0x00040000,  0x06, 0x05, 0x05, 0x10)
TEST_EXEC(test_dec_c,               REG_BC, 0x00000004,  0x0e, 0x05, 0x0d, 0x10)
TEST_EXEC(test_inc_c,               REG_BC, 0x00000007,  0x0e, 0x06, 0x0c, 0x10)
TEST_EXEC(test_inc_e,               REG_DE, 0x00000006,  0x1e, 0x05, 0x1c, 0x10)
TEST_EXEC(test_xor_a,               REG_A,  0x0,  0x3e, 0x20, 0xaf, 0x10)

TEST_EXEC(test_dec_bc,              REG_BC, 0x00110021, 0x01, 0x22, 0x11, 0x0b, 0x10)

TEST_EXEC(test_ld_a_b,              REG_A,  0x05, 0x06, 0x05, 0x78, 0x10)
TEST_EXEC(test_ld_b_a,              REG_BC, 0x00110000, 0x3e, 0x11, 0x47, 0x10)
TEST_EXEC(test_ld_a_c,              REG_A,  0x05, 0x0e, 0x05, 0x79, 0x10)
TEST_EXEC(test_ld_c_a,              REG_BC, 0x00000011, 0x3e, 0x11, 0x4f, 0x10)
TEST_EXEC(test_ld_d_a,              REG_DE, 0x00ab0000, 0x3e, 0xab, 0x57, 0x10)

// ld a, $10; ld c, $01; or a, c
TEST_EXEC(test_or_a_c,              REG_A,  0x11, 0x3e, 0x10, 0x0e, 0x01, 0xb1, 0x10)

// ld a, $f0; cpl -> A = 0x0f
TEST_EXEC(test_cpl,                 REG_A,  0x0f, 0x3e, 0xf0, 0x2f, 0x10)

// ld a, $ff; and a, $0f -> A = 0x0f
TEST_EXEC(test_and_a_imm,           REG_A,  0x0f, 0x3e, 0xff, 0xe6, 0x0f, 0x10)

// ld a, $12; swap a -> A = 0x21
TEST_EXEC(test_swap_a,              REG_A,  0x21, 0x3e, 0x12, 0xcb, 0x37, 0x10)

// RLC tests - rotate left circular (bit 7 -> C and bit 0)
// ld a, $80; rlc a -> A = 0x01, C = 1
TEST_EXEC(test_rlc_a,               REG_A,  0x01, 0x3e, 0x80, 0xcb, 0x07, 0x10)
// ld a, $81; rlc a -> A = 0x03, C = 1
TEST_EXEC(test_rlc_a_both_bits,     REG_A,  0x03, 0x3e, 0x81, 0xcb, 0x07, 0x10)
// ld b, $40; rlc b -> B = 0x80, C = 0
TEST_EXEC(test_rlc_b,               REG_BC, 0x00800000, 0x06, 0x40, 0xcb, 0x00, 0x10)
// ld c, $01; rlc c -> C = 0x02
TEST_EXEC(test_rlc_c,               REG_BC, 0x00000002, 0x0e, 0x01, 0xcb, 0x01, 0x10)

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

// SWAP tests for other registers
// ld b, $ab; swap b -> B = 0xba
TEST_EXEC(test_swap_b,              REG_BC, 0x00ba0000, 0x06, 0xab, 0xcb, 0x30, 0x10)
// ld c, $12; swap c -> C = 0x21
TEST_EXEC(test_swap_c,              REG_BC, 0x00000021, 0x0e, 0x12, 0xcb, 0x31, 0x10)
// ld d, $f0; swap d -> D = 0x0f
TEST_EXEC(test_swap_d,              REG_DE, 0x000f0000, 0x16, 0xf0, 0xcb, 0x32, 0x10)
// ld e, $34; swap e -> E = 0x43
TEST_EXEC(test_swap_e,              REG_DE, 0x00000043, 0x1e, 0x34, 0xcb, 0x33, 0x10)

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
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x10, 0x10);  // C flag set
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
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x10, 0x10);  // C flag set
}

// H and L register tests
// ld hl, $1234; swap h -> H = 0x21, HL = 0x2134
TEST_EXEC(test_swap_h,              REG_HL, 0x2134, 0x21, 0x34, 0x12, 0xcb, 0x34, 0x10)
// ld hl, $1234; swap l -> L = 0x43, HL = 0x1243
TEST_EXEC(test_swap_l,              REG_HL, 0x1243, 0x21, 0x34, 0x12, 0xcb, 0x35, 0x10)
// ld hl, $8080; rlc h -> H = 0x01 (bit 7 wraps to bit 0), HL = 0x0180
TEST_EXEC(test_rlc_h,               REG_HL, 0x0180, 0x21, 0x80, 0x80, 0xcb, 0x04, 0x10)
// ld hl, $8001; rlc l -> L = 0x02, HL = 0x8002
TEST_EXEC(test_rlc_l,               REG_HL, 0x8002, 0x21, 0x01, 0x80, 0xcb, 0x05, 0x10)
// ld hl, $0202; srl h -> H = 0x01, HL = 0x0102
TEST_EXEC(test_srl_h,               REG_HL, 0x0102, 0x21, 0x02, 0x02, 0xcb, 0x3c, 0x10)
// ld hl, $0104; srl l -> L = 0x02, HL = 0x0102
TEST_EXEC(test_srl_l,               REG_HL, 0x0102, 0x21, 0x04, 0x01, 0xcb, 0x3d, 0x10)

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
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x80, 0x80);  // Z flag set
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
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x10, 0x10);  // C flag set
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x80, 0x80);  // Z flag set
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
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x10, 0x00);  // C flag clear
}

// ld a, $10; ld b, $01; or a, b -> A = 0x11
TEST_EXEC(test_or_a_b,              REG_A,  0x11, 0x3e, 0x10, 0x06, 0x01, 0xb0, 0x10)

// ld a, $ff; ld c, $0f; xor a, c -> A = 0xf0
TEST_EXEC(test_xor_a_c,             REG_A,  0xf0, 0x3e, 0xff, 0x0e, 0x0f, 0xa9, 0x10)

// ld a, $ff; ld c, $0f; and a, c -> A = 0x0f
TEST_EXEC(test_and_a_c,             REG_A,  0x0f, 0x3e, 0xff, 0x0e, 0x0f, 0xa1, 0x10)

// ld a, $42; and a, a -> A = 0x42, Z = 0
TEST_EXEC(test_and_a_a_nonzero,     REG_A,  0x42, 0x3e, 0x42, 0xa7, 0x10)

// ld a, $00; and a, a -> A = 0x00, Z = 1
TEST_EXEC(test_and_a_a_zero,        REG_A,  0x00, 0x3e, 0x00, 0xa7, 0x10)

// ld a, $20; add a, a -> A = 0x40
TEST_EXEC(test_add_a_a,             REG_A,  0x40, 0x3e, 0x20, 0x87, 0x10)

// ld a, $10; ld d, $05; add a, d -> A = 0x15
TEST_EXEC(test_add_a_d,             REG_A,  0x15, 0x3e, 0x10, 0x16, 0x05, 0x82, 0x10)

// Test ADD sets carry and ADC uses it (bug fix verification)
TEST(test_add_adc_carry_propagation)
{
    // add a, b overflows: $f0 + $20 = $110, A = $10, C = 1
    // adc a, c uses carry: $10 + $00 + 1 = $11
    uint8_t rom[] = {
        0x3e, 0xf0,       // ld a, $f0
        0x06, 0x20,       // ld b, $20
        0x80,             // add a, b -> A = $10, C = 1
        0x0e, 0x00,       // ld c, $00
        0x89,             // adc a, c -> A = $10 + $00 + 1 = $11
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x11);
}

// ld e, $ab; ld a, $cd; ld e, a -> E = 0xcd
TEST_EXEC(test_ld_e_a,              REG_DE, 0x000000cd, 0x1e, 0xab, 0x3e, 0xcd, 0x5f, 0x10)

// ld hl, $1234; inc l -> HL = 0x1235
TEST_EXEC(test_inc_l,               REG_HL, 0x1235, 0x21, 0x34, 0x12, 0x2c, 0x10)

// ld hl, $1234; dec l -> HL = 0x1233
TEST_EXEC(test_dec_l,               REG_HL, 0x1233, 0x21, 0x34, 0x12, 0x2d, 0x10)

// ld hl, $1234; inc hl -> HL = 0x1235
TEST_EXEC(test_inc_hl,              REG_HL, 0x1235, 0x21, 0x34, 0x12, 0x23, 0x10)

// ld hl, $1000; ld de, $0234; add hl, de -> HL = 0x1234
TEST_EXEC(test_add_hl_de,           REG_HL, 0x1234, 0x21, 0x00, 0x10, 0x11, 0x34, 0x02, 0x19, 0x10)

// ADC tests
TEST(test_adc_a_c_no_carry)
{
    // xor a (clear carry), ld a, $10, ld c, $05, adc a, c -> A = 0x15
    uint8_t rom[] = {
        0xaf,             // xor a (A=0, C=0)
        0x3e, 0x10,       // ld a, $10
        0x0e, 0x05,       // ld c, $05
        0x89,             // adc a, c
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x15);
}

TEST(test_adc_a_c_with_carry)
{
    // Set carry via cp, then adc
    uint8_t rom[] = {
        0x3e, 0x00,       // ld a, $00
        0xfe, 0x01,       // cp a, $01 -> C=1 (0 < 1)
        0x3e, 0x10,       // ld a, $10
        0x0e, 0x05,       // ld c, $05
        0x89,             // adc a, c -> A = $10 + $05 + 1 = $16
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x16);
}

TEST(test_adc_a_imm_with_carry)
{
    // Set carry, then adc a, #imm
    uint8_t rom[] = {
        0x3e, 0x00,       // ld a, $00
        0xfe, 0x01,       // cp a, $01 -> C=1
        0x3e, 0x20,       // ld a, $20
        0xce, 0x10,       // adc a, $10 -> A = $20 + $10 + 1 = $31
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x31);
}

TEST(test_adc_overflow_sets_carry)
{
    // adc that overflows should set carry
    uint8_t rom[] = {
        0xaf,             // xor a (C=0)
        0x3e, 0xff,       // ld a, $ff
        0x0e, 0x01,       // ld c, $01
        0x89,             // adc a, c -> $ff + $01 = $100, A = $00, C = 1
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x00);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x10, 0x10);  // C flag set
}

TEST(test_adc_zero_flag)
{
    // adc resulting in zero should set Z
    uint8_t rom[] = {
        0xaf,             // xor a (C=0)
        0x3e, 0xff,       // ld a, $ff
        0x0e, 0x01,       // ld c, $01
        0x89,             // adc a, c -> A = $00, Z = 1
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x80, 0x80);  // Z flag set
}

// SBC tests
TEST(test_sbc_a_c_no_carry)
{
    // xor a (clear carry), ld a, $10, ld c, $05, sbc a, c -> A = 0x0b
    uint8_t rom[] = {
        0xaf,             // xor a (A=0, C=0)
        0x3e, 0x10,       // ld a, $10
        0x0e, 0x05,       // ld c, $05
        0x99,             // sbc a, c
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x0b);
}

TEST(test_sbc_a_c_with_carry)
{
    // Set carry via cp, then sbc
    uint8_t rom[] = {
        0x3e, 0x00,       // ld a, $00
        0xfe, 0x01,       // cp a, $01 -> C=1 (borrow)
        0x3e, 0x10,       // ld a, $10
        0x0e, 0x05,       // ld c, $05
        0x99,             // sbc a, c -> A = $10 - $05 - 1 = $0a
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x0a);
}

TEST(test_sbc_a_imm_with_carry)
{
    // Set carry, then sbc a, #imm
    uint8_t rom[] = {
        0x3e, 0x00,       // ld a, $00
        0xfe, 0x01,       // cp a, $01 -> C=1
        0x3e, 0x20,       // ld a, $20
        0xde, 0x10,       // sbc a, $10 -> A = $20 - $10 - 1 = $0f
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x0f);
}

TEST(test_sbc_underflow_sets_carry)
{
    // sbc that underflows should set carry
    uint8_t rom[] = {
        0xaf,             // xor a (C=0)
        0x3e, 0x00,       // ld a, $00
        0x0e, 0x01,       // ld c, $01
        0x99,             // sbc a, c -> $00 - $01 = $ff, C = 1
        0x10              // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xff);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x10, 0x10);  // C flag set
}

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
    // A = 1, B = 0, C = 3
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 1);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0x00ff00ff, 0x03);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x11);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x33);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0xbb);
}

TEST(test_exec_dec_a_loop)
{
    // Classic countdown loop: ld a,5; loop: dec a; jr nz,loop
    // A starts at 5, loop runs 5 times until A=0
    uint8_t rom[] = {
        0x3e, 0x05,       // 0x0000: ld a, 5
        0x3d,             // 0x0002: dec a (loop start)
        0x20, 0xfd,       // 0x0003: jr nz, -3 (back to 0x0002)
        0x10              // 0x0005: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x00);  // A should be 0 after loop
}

TEST(test_exec_cp_equal)
{
    // cp a, imm8 when equal - Z flag should be set
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x42,       // 0x0002: cp a, 0x42
        0x10              // 0x0004: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);  // A unchanged by cp
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x80, 0x80);  // Z flag set (bit 7)
}

TEST(test_exec_cp_not_equal)
{
    // cp a, imm8 when not equal - Z flag should be clear
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x10,       // 0x0002: cp a, 0x10
        0x10              // 0x0004: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);  // A unchanged
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x80, 0x00);  // Z flag clear
}

TEST(test_exec_cp_carry)
{
    // cp a, imm8 when A < imm8 - C flag should be set
    uint8_t rom[] = {
        0x3e, 0x10,       // 0x0000: ld a, 0x10
        0xfe, 0x42,       // 0x0002: cp a, 0x42 (0x10 < 0x42)
        0x10              // 0x0004: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x10, 0x10);  // C flag set (bit 4)
}

TEST(test_exec_cp_hl_equal)
{
    // cp a, (hl) when equal - Z flag should be set
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // 0x0000: ld hl, 0x5000
        0x36, 0x42,       // 0x0003: ld (hl), 0x42
        0x3e, 0x42,       // 0x0005: ld a, 0x42
        0xbe,             // 0x0007: cp a, (hl)
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);  // A unchanged by cp
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x80, 0x80);  // Z flag set
}

TEST(test_exec_cp_hl_not_equal)
{
    // cp a, (hl) when not equal - Z flag should be clear
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // 0x0000: ld hl, 0x5000
        0x36, 0x10,       // 0x0003: ld (hl), 0x10
        0x3e, 0x42,       // 0x0005: ld a, 0x42
        0xbe,             // 0x0007: cp a, (hl)
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);  // A unchanged
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x80, 0x00);  // Z flag clear
}

TEST(test_exec_cp_hl_carry)
{
    // cp a, (hl) when A < (hl) - C flag should be set
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // 0x0000: ld hl, 0x5000
        0x36, 0x42,       // 0x0003: ld (hl), 0x42
        0x3e, 0x10,       // 0x0005: ld a, 0x10
        0xbe,             // 0x0007: cp a, (hl) (0x10 < 0x42)
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x10, 0x10);  // C flag set
}

TEST(test_exec_jr_z_taken)
{
    // jr z jumps when Z=1 (after cp equal)
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x42,       // 0x0002: cp a, 0x42 (sets Z=1)
        0x28, 0x02,       // 0x0004: jr z, +2 (skip to 0x0008)
        0x3e, 0x00,       // 0x0006: ld a, 0x00 (skipped)
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);  // A unchanged (skip was taken)
}

TEST(test_exec_jr_z_not_taken)
{
    // jr z doesn't jump when Z=0 (after cp not equal)
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x10,       // 0x0002: cp a, 0x10 (sets Z=0)
        0x28, 0x02,       // 0x0004: jr z, +2 (not taken)
        0x3e, 0x99,       // 0x0006: ld a, 0x99 (executed)
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x99);  // A changed (skip not taken)
}

TEST(test_exec_jr_nz_then_call)
{
    uint8_t rom[] = {
        0x3e, 0x10,       // 0x0000 ld a, 10
        0xfe, 0x10,       // 0x0002: cp a, 10 (sets Z=1)
        0x20, 0xfd,       // 0x0004 jr nz, -5
        0xcd, 0x0a, 0x00, // 0x0006 call 000a
        0x10,             // 0x0009 stop
        0x3e, 0x99,       // 0x000a ld a, $99
        0xc9,             // 0x000c ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x99);
}

TEST(test_exec_jr_c_taken)
{
    // jr c jumps when C=1 (after cp with A < imm)
    uint8_t rom[] = {
        0x3e, 0x10,       // 0x0000: ld a, 0x10
        0xfe, 0x42,       // 0x0002: cp a, 0x42 (0x10 < 0x42, sets C=1)
        0x38, 0x02,       // 0x0004: jr c, +2 (skip to 0x0008)
        0x3e, 0x00,       // 0x0006: ld a, 0x00 (skipped)
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x10);  // A unchanged (skip was taken)
}

TEST(test_exec_jr_c_not_taken)
{
    // jr c doesn't jump when C=0 (after cp with A >= imm)
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x10,       // 0x0002: cp a, 0x10 (0x42 >= 0x10, sets C=0)
        0x38, 0x02,       // 0x0004: jr c, +2 (not taken)
        0x3e, 0x99,       // 0x0006: ld a, 0x99 (executed)
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x99);  // A changed (skip not taken)
}

TEST(test_exec_jr_nc_taken)
{
    // jr nc jumps when C=0 (after cp with A >= imm)
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x10,       // 0x0002: cp a, 0x10 (0x42 >= 0x10, sets C=0)
        0x30, 0x02,       // 0x0004: jr nc, +2 (skip to 0x0008)
        0x3e, 0x00,       // 0x0006: ld a, 0x00 (skipped)
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);  // A unchanged (skip was taken)
}

TEST(test_exec_jr_nc_not_taken)
{
    // jr nc doesn't jump when C=1 (after cp with A < imm)
    uint8_t rom[] = {
        0x3e, 0x10,       // 0x0000: ld a, 0x10
        0xfe, 0x42,       // 0x0002: cp a, 0x42 (0x10 < 0x42, sets C=1)
        0x30, 0x02,       // 0x0004: jr nc, +2 (not taken)
        0x3e, 0x99,       // 0x0006: ld a, 0x99 (executed)
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x99);  // A changed (skip not taken)
}

TEST(test_exec_and_a_a_with_jr)
{
    // and a, a sets Z flag correctly for conditional jump
    uint8_t rom[] = {
        0x3e, 0x00,       // 0x0000: ld a, 0x00
        0xa7,             // 0x0002: and a, a (sets Z=1)
        0x28, 0x02,       // 0x0003: jr z, +2 (taken)
        0x0e, 0x11,       // 0x0005: ld c, 0x11 (skipped)
        0x0e, 0x22,       // 0x0007: ld c, 0x22
        0x10              // 0x0009: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x22);  // jumped over 0x11
}

TEST(test_exec_jp_z_taken)
{
    // jp z jumps when Z=1
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x42,       // 0x0002: cp a, 0x42 (sets Z=1)
        0xca, 0x0a, 0x00, // 0x0004: jp z, 0x000a
        0x3e, 0x00,       // 0x0007: ld a, 0x00 (skipped)
        0x10,             // 0x0009: stop (skipped)
        0x0e, 0x99,       // 0x000a: ld c, 0x99
        0x10              // 0x000c: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);  // A unchanged (jump was taken)
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x99); // C set at target
}

TEST(test_exec_jp_z_not_taken)
{
    // jp z doesn't jump when Z=0
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x10,       // 0x0002: cp a, 0x10 (sets Z=0)
        0xca, 0x0a, 0x00, // 0x0004: jp z, 0x000a (not taken)
        0x3e, 0x99,       // 0x0007: ld a, 0x99 (executed)
        0x10,             // 0x0009: stop
        0x3e, 0x00,       // 0x000a: ld a, 0x00 (not reached)
        0x10              // 0x000c: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x99);  // A changed (fall-through)
}

TEST(test_exec_jp_nz_taken)
{
    // jp nz jumps when Z=0
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x10,       // 0x0002: cp a, 0x10 (sets Z=0)
        0xc2, 0x0a, 0x00, // 0x0004: jp nz, 0x000a
        0x3e, 0x00,       // 0x0007: ld a, 0x00 (skipped)
        0x10,             // 0x0009: stop (skipped)
        0x0e, 0x88,       // 0x000a: ld c, 0x88
        0x10              // 0x000c: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);  // A unchanged (jump was taken)
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x88); // C set at target
}

TEST(test_exec_jp_nz_not_taken)
{
    // jp nz doesn't jump when Z=1
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x42,       // 0x0002: cp a, 0x42 (sets Z=1)
        0xc2, 0x0a, 0x00, // 0x0004: jp nz, 0x000a (not taken)
        0x3e, 0x77,       // 0x0007: ld a, 0x77 (executed)
        0x10,             // 0x0009: stop
        0x3e, 0x00,       // 0x000a: ld a, 0x00 (not reached)
        0x10              // 0x000c: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x77);  // A changed (fall-through)
}

TEST(test_exec_jp_c_taken)
{
    // jp c jumps when C=1
    uint8_t rom[] = {
        0x3e, 0x10,       // 0x0000: ld a, 0x10
        0xfe, 0x42,       // 0x0002: cp a, 0x42 (0x10 < 0x42, sets C=1)
        0xda, 0x0a, 0x00, // 0x0004: jp c, 0x000a
        0x3e, 0x00,       // 0x0007: ld a, 0x00 (skipped)
        0x10,             // 0x0009: stop (skipped)
        0x0e, 0x55,       // 0x000a: ld c, 0x55
        0x10              // 0x000c: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x10);  // A unchanged (jump was taken)
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x55); // C set at target
}

TEST(test_exec_jp_c_not_taken)
{
    // jp c doesn't jump when C=0
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x10,       // 0x0002: cp a, 0x10 (0x42 >= 0x10, sets C=0)
        0xda, 0x0a, 0x00, // 0x0004: jp c, 0x000a (not taken)
        0x3e, 0x66,       // 0x0007: ld a, 0x66 (executed)
        0x10,             // 0x0009: stop
        0x3e, 0x00,       // 0x000a: ld a, 0x00 (not reached)
        0x10              // 0x000c: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x66);  // A changed (fall-through)
}

TEST(test_exec_jp_nc_taken)
{
    // jp nc jumps when C=0
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x10,       // 0x0002: cp a, 0x10 (0x42 >= 0x10, sets C=0)
        0xd2, 0x0a, 0x00, // 0x0004: jp nc, 0x000a
        0x3e, 0x00,       // 0x0007: ld a, 0x00 (skipped)
        0x10,             // 0x0009: stop (skipped)
        0x0e, 0x44,       // 0x000a: ld c, 0x44
        0x10              // 0x000c: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);  // A unchanged (jump was taken)
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x44); // C set at target
}

TEST(test_exec_jp_nc_not_taken)
{
    // jp nc doesn't jump when C=1
    uint8_t rom[] = {
        0x3e, 0x10,       // 0x0000: ld a, 0x10
        0xfe, 0x42,       // 0x0002: cp a, 0x42 (0x10 < 0x42, sets C=1)
        0xd2, 0x0a, 0x00, // 0x0004: jp nc, 0x000a (not taken)
        0x3e, 0xaa,       // 0x0007: ld a, 0xaa (executed)
        0x10,             // 0x0009: stop
        0x3e, 0x00,       // 0x000a: ld a, 0x00 (not reached)
        0x10              // 0x000c: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);  // A changed (fall-through)
}

TEST(test_exec_call_ret_simple)
{
    // Simple call and return
    uint8_t rom[] = {
        0x3e, 0x11,       // 0x0000: ld a, 0x11
        0xcd, 0x08, 0x00, // 0x0002: call 0x0008
        0x3e, 0x33,       // 0x0005: ld a, 0x33 (after return)
        0x10,             // 0x0007: stop
        // subroutine at 0x0008:
        0x06, 0x22,       // 0x0008: ld b, 0x22
        0xc9              // 0x000a: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x33);        // A = 0x33 (set after return)
    ASSERT_EQ((get_dreg(REG_68K_D_BC) >> 16) & 0xff, 0x22); // B = 0x22 (set in subroutine)
}

TEST(test_exec_ret_nz_taken)
{
    // ret nz returns when Z=0
    uint8_t rom[] = {
        0xcd, 0x07, 0x00, // 0x0000: call 0x0007
        0x3e, 0xaa,       // 0x0003: ld a, 0xaa (after return)
        0x10,             // 0x0005: stop
        0x00,             // 0x0006: padding
        // subroutine at 0x0007:
        0x3e, 0x42,       // 0x0007: ld a, 0x42
        0xfe, 0x10,       // 0x0009: cp a, 0x10 (sets Z=0)
        0xc0,             // 0x000b: ret nz (taken)
        0x3e, 0x00,       // 0x000c: ld a, 0x00 (skipped)
        0xc9              // 0x000e: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);  // returned early, then set to 0xaa
}

TEST(test_exec_ret_nz_not_taken)
{
    // ret nz doesn't return when Z=1
    uint8_t rom[] = {
        0xcd, 0x07, 0x00, // 0x0000: call 0x0007
        0x3e, 0xaa,       // 0x0003: ld a, 0xaa (after return)
        0x10,             // 0x0005: stop
        0x00,             // 0x0006: padding
        // subroutine at 0x0007:
        0x3e, 0x42,       // 0x0007: ld a, 0x42
        0xfe, 0x42,       // 0x0009: cp a, 0x42 (sets Z=1)
        0xc0,             // 0x000b: ret nz (not taken)
        0x0e, 0x99,       // 0x000c: ld c, 0x99
        0xc9              // 0x000e: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x99);  // C was set (ret nz not taken)
}

TEST(test_exec_ret_z_taken)
{
    // ret z returns when Z=1
    uint8_t rom[] = {
        0xcd, 0x07, 0x00, // 0x0000: call 0x0007
        0x3e, 0xbb,       // 0x0003: ld a, 0xbb (after return)
        0x10,             // 0x0005: stop
        0x00,             // 0x0006: padding
        // subroutine at 0x0007:
        0x3e, 0x42,       // 0x0007: ld a, 0x42
        0xfe, 0x42,       // 0x0009: cp a, 0x42 (sets Z=1)
        0xc8,             // 0x000b: ret z (taken)
        0x3e, 0x00,       // 0x000c: ld a, 0x00 (skipped)
        0xc9              // 0x000e: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xbb);  // returned early, then set to 0xbb
}

TEST(test_exec_ret_z_not_taken)
{
    // ret z doesn't return when Z=0
    uint8_t rom[] = {
        0xcd, 0x07, 0x00, // 0x0000: call 0x0007
        0x3e, 0xbb,       // 0x0003: ld a, 0xbb (after return)
        0x10,             // 0x0005: stop
        0x00,             // 0x0006: padding
        // subroutine at 0x0007:
        0x3e, 0x42,       // 0x0007: ld a, 0x42
        0xfe, 0x10,       // 0x0009: cp a, 0x10 (sets Z=0)
        0xc8,             // 0x000b: ret z (not taken)
        0x0e, 0x77,       // 0x000c: ld c, 0x77
        0xc9              // 0x000e: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xbb);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x77);  // C was set (ret z not taken)
}

TEST(test_exec_ret_nc_taken)
{
    // ret nc returns when C=0
    uint8_t rom[] = {
        0xcd, 0x07, 0x00, // 0x0000: call 0x0007
        0x3e, 0xcc,       // 0x0003: ld a, 0xcc (after return)
        0x10,             // 0x0005: stop
        0x00,             // 0x0006: padding
        // subroutine at 0x0007:
        0x3e, 0x42,       // 0x0007: ld a, 0x42
        0xfe, 0x10,       // 0x0009: cp a, 0x10 (0x42 >= 0x10, sets C=0)
        0xd0,             // 0x000b: ret nc (taken)
        0x3e, 0x00,       // 0x000c: ld a, 0x00 (skipped)
        0xc9              // 0x000e: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xcc);  // returned early, then set to 0xcc
}

TEST(test_exec_ret_nc_not_taken)
{
    // ret nc doesn't return when C=1
    uint8_t rom[] = {
        0xcd, 0x07, 0x00, // 0x0000: call 0x0007
        0x3e, 0xcc,       // 0x0003: ld a, 0xcc (after return)
        0x10,             // 0x0005: stop
        0x00,             // 0x0006: padding
        // subroutine at 0x0007:
        0x3e, 0x10,       // 0x0007: ld a, 0x10
        0xfe, 0x42,       // 0x0009: cp a, 0x42 (0x10 < 0x42, sets C=1)
        0xd0,             // 0x000b: ret nc (not taken)
        0x0e, 0x55,       // 0x000c: ld c, 0x55
        0xc9              // 0x000e: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xcc);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x55);  // C was set (ret nc not taken)
}

TEST(test_exec_ret_c_taken)
{
    // ret c returns when C=1
    uint8_t rom[] = {
        0xcd, 0x07, 0x00, // 0x0000: call 0x0007
        0x3e, 0xdd,       // 0x0003: ld a, 0xdd (after return)
        0x10,             // 0x0005: stop
        0x00,             // 0x0006: padding
        // subroutine at 0x0007:
        0x3e, 0x10,       // 0x0007: ld a, 0x10
        0xfe, 0x42,       // 0x0009: cp a, 0x42 (0x10 < 0x42, sets C=1)
        0xd8,             // 0x000b: ret c (taken)
        0x3e, 0x00,       // 0x000c: ld a, 0x00 (skipped)
        0xc9              // 0x000e: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xdd);  // returned early, then set to 0xdd
}

TEST(test_exec_ret_c_not_taken)
{
    // ret c doesn't return when C=0
    uint8_t rom[] = {
        0xcd, 0x07, 0x00, // 0x0000: call 0x0007
        0x3e, 0xdd,       // 0x0003: ld a, 0xdd (after return)
        0x10,             // 0x0005: stop
        0x00,             // 0x0006: padding
        // subroutine at 0x0007:
        0x3e, 0x42,       // 0x0007: ld a, 0x42
        0xfe, 0x10,       // 0x0009: cp a, 0x10 (0x42 >= 0x10, sets C=0)
        0xd8,             // 0x000b: ret c (not taken)
        0x0e, 0x66,       // 0x000c: ld c, 0x66
        0xc9              // 0x000e: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xdd);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x66);  // C was set (ret c not taken)
}

TEST(test_exec_call_nz_taken)
{
    // call nz calls when Z=0
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x10,       // 0x0002: cp a, 0x10 (sets Z=0)
        0xc4, 0x0b, 0x00, // 0x0004: call nz, 0x000b (taken)
        0x3e, 0xaa,       // 0x0007: ld a, 0xaa (after return)
        0x10,             // 0x0009: stop
        0x00,             // 0x000a: padding
        // subroutine at 0x000b:
        0x0e, 0x77,       // 0x000b: ld c, 0x77
        0xc9              // 0x000d: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x77);  // C was set (call was taken)
}

TEST(test_exec_call_nz_not_taken)
{
    // call nz doesn't call when Z=1
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x42,       // 0x0002: cp a, 0x42 (sets Z=1)
        0xc4, 0x0b, 0x00, // 0x0004: call nz, 0x000b (not taken)
        0x3e, 0xaa,       // 0x0007: ld a, 0xaa
        0x10,             // 0x0009: stop
        0x00,             // 0x000a: padding
        // subroutine at 0x000b:
        0x0e, 0x77,       // 0x000b: ld c, 0x77
        0xc9              // 0x000d: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x00);  // C not set (call not taken)
}

TEST(test_exec_call_z_taken)
{
    // call z calls when Z=1
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x42,       // 0x0002: cp a, 0x42 (sets Z=1)
        0xcc, 0x0b, 0x00, // 0x0004: call z, 0x000b (taken)
        0x3e, 0xbb,       // 0x0007: ld a, 0xbb (after return)
        0x10,             // 0x0009: stop
        0x00,             // 0x000a: padding
        // subroutine at 0x000b:
        0x0e, 0x88,       // 0x000b: ld c, 0x88
        0xc9              // 0x000d: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xbb);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x88);  // C was set (call was taken)
}

TEST(test_exec_call_z_not_taken)
{
    // call z doesn't call when Z=0
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x10,       // 0x0002: cp a, 0x10 (sets Z=0)
        0xcc, 0x0b, 0x00, // 0x0004: call z, 0x000b (not taken)
        0x3e, 0xbb,       // 0x0007: ld a, 0xbb
        0x10,             // 0x0009: stop
        0x00,             // 0x000a: padding
        // subroutine at 0x000b:
        0x0e, 0x88,       // 0x000b: ld c, 0x88
        0xc9              // 0x000d: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xbb);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x00);  // C not set (call not taken)
}

TEST(test_exec_call_nc_taken)
{
    // call nc calls when C=0
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x10,       // 0x0002: cp a, 0x10 (0x42 >= 0x10, sets C=0)
        0xd4, 0x0b, 0x00, // 0x0004: call nc, 0x000b (taken)
        0x3e, 0xcc,       // 0x0007: ld a, 0xcc (after return)
        0x10,             // 0x0009: stop
        0x00,             // 0x000a: padding
        // subroutine at 0x000b:
        0x0e, 0x99,       // 0x000b: ld c, 0x99
        0xc9              // 0x000d: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xcc);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x99);  // C was set (call was taken)
}

TEST(test_exec_call_nc_not_taken)
{
    // call nc doesn't call when C=1
    uint8_t rom[] = {
        0x3e, 0x10,       // 0x0000: ld a, 0x10
        0xfe, 0x42,       // 0x0002: cp a, 0x42 (0x10 < 0x42, sets C=1)
        0xd4, 0x0b, 0x00, // 0x0004: call nc, 0x000b (not taken)
        0x3e, 0xcc,       // 0x0007: ld a, 0xcc
        0x10,             // 0x0009: stop
        0x00,             // 0x000a: padding
        // subroutine at 0x000b:
        0x0e, 0x99,       // 0x000b: ld c, 0x99
        0xc9              // 0x000d: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xcc);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x00);  // C not set (call not taken)
}

TEST(test_exec_call_c_taken)
{
    // call c calls when C=1
    uint8_t rom[] = {
        0x3e, 0x10,       // 0x0000: ld a, 0x10
        0xfe, 0x42,       // 0x0002: cp a, 0x42 (0x10 < 0x42, sets C=1)
        0xdc, 0x0b, 0x00, // 0x0004: call c, 0x000b (taken)
        0x3e, 0xdd,       // 0x0007: ld a, 0xdd (after return)
        0x10,             // 0x0009: stop
        0x00,             // 0x000a: padding
        // subroutine at 0x000b:
        0x0e, 0x55,       // 0x000b: ld c, 0x55
        0xc9              // 0x000d: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xdd);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x55);  // C was set (call was taken)
}

TEST(test_exec_call_c_not_taken)
{
    // call c doesn't call when C=0
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xfe, 0x10,       // 0x0002: cp a, 0x10 (0x42 >= 0x10, sets C=0)
        0xdc, 0x0b, 0x00, // 0x0004: call c, 0x000b (not taken)
        0x3e, 0xdd,       // 0x0007: ld a, 0xdd
        0x10,             // 0x0009: stop
        0x00,             // 0x000a: padding
        // subroutine at 0x000b:
        0x0e, 0x55,       // 0x000b: ld c, 0x55
        0xc9              // 0x000d: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xdd);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x00);  // C not set (call not taken)
}

TEST(test_exec_call_ret_nested)
{
    // Nested calls: main -> sub1 -> sub2
    uint8_t rom[] = {
        0x3e, 0x01,       // 0x0000: ld a, 1
        0xcd, 0x08, 0x00, // 0x0002: call sub1 (0x0008)
        0x3e, 0x04,       // 0x0005: ld a, 4
        0x10,             // 0x0007: stop
        // sub1 at 0x0008:
        0x3e, 0x02,       // 0x0008: ld a, 2
        0xcd, 0x10, 0x00, // 0x000a: call sub2 (0x0010)
        0x3e, 0x03,       // 0x000d: ld a, 3
        0xc9,             // 0x000f: ret from sub1
        // sub2 at 0x0010:
        0x0e, 0x99,       // 0x0010: ld c, 0x99
        0xc9              // 0x0012: ret from sub2
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x04);  // A = 4 (final value after all returns)
}

TEST(test_exec_call_preserves_regs)
{
    // Call should not clobber registers other than what subroutine modifies
    uint8_t rom[] = {
        0x3e, 0xaa,       // 0x0000: ld a, 0xaa
        0x06, 0xbb,       // 0x0002: ld b, 0xbb
        0xcd, 0x0a, 0x00, // 0x0004: call 0x000a
        0x10,             // 0x0007: stop
        0x00, 0x00,       // padding
        // subroutine at 0x000a:
        0x0e, 0xcc,       // 0x000a: ld c, 0xcc
        0xc9              // 0x000c: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);         // A preserved
    ASSERT_EQ((get_dreg(REG_68K_D_BC) >> 16) & 0xff, 0xbb); // B preserved
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0xcc);         // C set by subroutine
}

TEST(test_exec_ld_bc_ind_a)
{
    // ld (bc), a - write A to memory address BC
    uint8_t rom[] = {
        0x01, 0x00, 0x50, // 0x0000: ld bc, 0x5000
        0x3e, 0x42,       // 0x0003: ld a, 0x42
        0x02,             // 0x0005: ld (bc), a
        0x10              // 0x0006: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_mem_byte(0x5000), 0x42);  // memory at BC should have A's value
}

TEST(test_exec_ld_a_bc_ind)
{
    // ld a, (bc) - read byte from memory address BC into A
    uint8_t rom[] = {
        0x01, 0x00, 0x50, // 0x0000: ld bc, 0x5000
        0x3e, 0x42,       // 0x0003: ld a, 0x42
        0x02,             // 0x0005: ld (bc), a  ; write 0x42 to memory
        0x3e, 0x00,       // 0x0006: ld a, 0x00  ; clear A
        0x0a,             // 0x0008: ld a, (bc)  ; read back from memory
        0x10              // 0x0009: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);  // A should have value read from memory
}

TEST(test_exec_ld_de_ind_a)
{
    // ld (de), a - write A to memory address DE
    uint8_t rom[] = {
        0x11, 0x00, 0x50, // 0x0000: ld de, 0x5000
        0x3e, 0x42,       // 0x0003: ld a, 0x42
        0x12,             // 0x0005: ld (de), a
        0x10              // 0x0006: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_mem_byte(0x5000), 0x42);  // memory at DE should have A's value
}

TEST(test_exec_ld_a_de_ind)
{
    // ld a, (de) - read byte from memory address DE into A
    uint8_t rom[] = {
        0x11, 0x00, 0x50, // 0x0000: ld de, 0x5000
        0x3e, 0x42,       // 0x0003: ld a, 0x42
        0x12,             // 0x0005: ld (de), a  ; write 0x42 to memory
        0x3e, 0x00,       // 0x0006: ld a, 0x00  ; clear A
        0x1a,             // 0x0008: ld a, (de)  ; read back from memory
        0x10              // 0x0009: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);  // A should have value read from memory
}

TEST(test_exec_ld_hld_a)
{
    // ld (hl-), a - write A to memory address HL, then decrement HL
    uint8_t rom[] = {
        0x21, 0x02, 0x50, // 0x0000: ld hl, 0x5002
        0x3e, 0xaa,       // 0x0003: ld a, 0xaa
        0x32,             // 0x0005: ld (hl-), a  ; write 0xaa to 0x5002, HL becomes 0x5001
        0x3e, 0xbb,       // 0x0006: ld a, 0xbb
        0x32,             // 0x0008: ld (hl-), a  ; write 0xbb to 0x5001, HL becomes 0x5000
        0x10              // 0x0009: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_mem_byte(0x5002), 0xaa);
    ASSERT_EQ(get_mem_byte(0x5001), 0xbb);
    ASSERT_EQ(get_areg(REG_68K_A_HL), 0x5000);  // HL decremented twice
}

TEST(test_exec_ld_a_hli)
{
    // ld a, (hl+) - write memory address HL to A, then increment HL
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // 0x0000: ld hl, 0x5000
        0x36, 0x11,       // 0x0003: ld (hl), 0x11
        0x2a,             // 0x0005: ld a, (hl+)
        0x10              // 0x0006: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A), 0x11);
    ASSERT_EQ(get_areg(REG_68K_A_HL), 0x5001);
}

TEST(test_exec_ldh_imm8_a)
{
    // ld ($ff00 + u8), a - write A to high memory
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xe0, 0x80,       // 0x0002: ld ($ff80), a
        0x3e, 0x99,       // 0x0004: ld a, 0x99
        0xe0, 0x81,       // 0x0006: ld ($ff81), a
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_mem_byte(0x4000), 0x42);
    ASSERT_EQ(get_mem_byte(0x4001), 0x99);
}

TEST(test_exec_ldh_c_a)
{
    // ld ($ff00 + c), a - write A to $ff00 + C
    uint8_t rom[] = {
        0x0e, 0x42,       // 0x0000: ld c, 0x42
        0x3e, 0xaa,       // 0x0002: ld a, 0xaa
        0xe2,             // 0x0004: ld ($ff00 + c), a
        0x0e, 0x43,       // 0x0005: ld c, 0x43
        0x3e, 0xbb,       // 0x0007: ld a, 0xbb
        0xe2,             // 0x0009: ld ($ff00 + c), a
        0x10              // 0x000a: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_mem_byte(0xff42), 0xaa);
    ASSERT_EQ(get_mem_byte(0xff43), 0xbb);
}

TEST(test_exec_ldh_a_imm8)
{
    // ld a, ($ff00 + u8) - read from high memory into A
    uint8_t rom[] = {
        0x3e, 0xab,       // 0x0000: ld a, 0xab
        0xe0, 0x90,       // 0x0002: ld ($ff90), a  ; write 0xab to $ff90
        0x3e, 0x00,       // 0x0004: ld a, 0x00     ; clear A
        0xf0, 0x90,       // 0x0006: ld a, ($ff90)  ; read back from $ff90
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xab);
}

TEST(test_exec_ld_hl_ind_imm8)
{
    // ld (hl), u8 - write immediate byte to memory at HL
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // 0x0000: ld hl, 0x5000
        0x36, 0x42,       // 0x0003: ld (hl), 0x42
        0x21, 0x01, 0x50, // 0x0005: ld hl, 0x5001
        0x36, 0x99,       // 0x0008: ld (hl), 0x99
        0x10              // 0x000a: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_mem_byte(0x5000), 0x42);
    ASSERT_EQ(get_mem_byte(0x5001), 0x99);
}

TEST(test_inc_hl_ind)
{
    // inc (hl) - increment byte at memory address HL
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // 0x0000: ld hl, 0x5000
        0x36, 0x41,       // 0x0003: ld (hl), 0x41
        0x34,             // 0x0005: inc (hl)
        0x10              // 0x0006: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_mem_byte(0x5000), 0x42);
}

TEST(test_dec_hl_ind)
{
    // dec (hl) - decrement byte at memory address HL
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // 0x0000: ld hl, 0x5000
        0x36, 0x43,       // 0x0003: ld (hl), 0x43
        0x35,             // 0x0005: dec (hl)
        0x10              // 0x0006: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_mem_byte(0x5000), 0x42);
}

TEST(test_ei)
{
    // ei - enable interrupts
    uint8_t rom[] = {
        0xfb,             // 0x0000: ei
        0x10              // 0x0001: stop
    };
    run_program(rom, 0);
    // big endian...
    ASSERT_EQ(get_mem_byte(U16_INTERRUPTS_ENABLED + 1), 1);
}

TEST(test_di)
{
    // di - disable interrupts
    uint8_t rom[] = {
        0x21, 0x00, 0x40, // 0x0000: ld hl, 0x4000
        0x36, 0x01,       // 0x0003: ld (hl), 0x01
        0xf3,             // 0x0000: di
        0x10              // 0x0001: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_mem_byte(U16_INTERRUPTS_ENABLED + 1), 0);
}

TEST(test_push_bc)
{
    // push bc, pop hl - transfer BC to HL via stack
    uint8_t rom[] = {
        0x01, 0x34, 0x12, // 0x0000: ld bc, 0x1234
        0xc5,             // 0x0003: push bc
        0xe1,             // 0x0004: pop hl
        0x10              // 0x0005: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_areg(REG_68K_A_HL), 0x1234);
}

TEST(test_push_de)
{
    // push de, pop hl - transfer DE to HL via stack
    uint8_t rom[] = {
        0x11, 0x34, 0x12, // 0x0000: ld de, 0x1234
        0xd5,             // 0x0003: push de
        0xe1,             // 0x0004: pop hl
        0x10              // 0x0005: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_areg(REG_68K_A_HL), 0x1234);
}

TEST(test_push_hl)
{
    // push hl, pop hl - verify stack round-trip
    uint8_t rom[] = {
        0x21, 0x78, 0x56, // 0x0000: ld hl, 0x5678
        0xe5,             // 0x0003: push hl
        0x21, 0x00, 0x00, // 0x0004: ld hl, 0x0000 (clear HL)
        0xe1,             // 0x0007: pop hl
        0x10              // 0x0008: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_areg(REG_68K_A_HL), 0x5678);
}

TEST(test_pop_de)
{
    // push hl, pop de - transfer HL to DE via stack
    uint8_t rom[] = {
        0x21, 0x34, 0x12, // 0x0000: ld hl, 0x1234
        0xe5,             // 0x0003: push hl
        0xd1,             // 0x0004: pop de
        0x10              // 0x0005: stop
    };
    run_program(rom, 0);
    // DE in split format: 0x00120034
    ASSERT_EQ(get_dreg(REG_68K_D_DE), 0x00120034);
}

TEST(test_jp_hl)
{
    // jp (hl) - jump to address in HL
    uint8_t rom[] = {
        0x21, 0x08, 0x00, // 0x0000: ld hl, 0x0008
        0xe9,             // 0x0003: jp (hl)
        0x3e, 0x11,       // 0x0004: ld a, 0x11 (skipped)
        0x10,             // 0x0006: stop (skipped)
        0x00,             // 0x0007: padding
        0x3e, 0x22,       // 0x0008: ld a, 0x22
        0x10              // 0x000a: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x22);
}

TEST(test_ld_hl_sp_plus_0)
{
    // ld hl, sp+0 - copy SP to HL
    uint8_t rom[] = {
        0x31, 0xf0, 0xdf, // 0x0000: ld sp, 0xdff0
        0xf8, 0x00,       // 0x0003: ld hl, sp+0
        0x10              // 0x0005: stop
    };
    run_program(rom, 0);
    // Mask to 16 bits (movea.w sign-extends for values >= 0x8000)
    ASSERT_EQ(get_areg(REG_68K_A_HL) & 0xffff, 0xdff0);
}

TEST(test_ld_hl_sp_plus_positive)
{
    // ld hl, sp+5 - SP + positive offset
    uint8_t rom[] = {
        0x31, 0xf0, 0xdf, // 0x0000: ld sp, 0xdff0
        0xf8, 0x05,       // 0x0003: ld hl, sp+5
        0x10              // 0x0005: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_areg(REG_68K_A_HL) & 0xffff, 0xdff5);
}

TEST(test_ld_hl_sp_plus_negative)
{
    // ld hl, sp-5 - SP + negative offset (0xfb = -5)
    uint8_t rom[] = {
        0x31, 0xf0, 0xdf, // 0x0000: ld sp, 0xdff0
        0xf8, 0xfb,       // 0x0003: ld hl, sp-5
        0x10              // 0x0005: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_areg(REG_68K_A_HL) & 0xffff, 0xdfeb);
}

TEST(test_ld_sp_hl_roundtrip)
{
    // Save SP to HL, push something, then restore SP
    uint8_t rom[] = {
        0x31, 0xf0, 0xdf, // 0x0000: ld sp, 0xdff0
        0xf8, 0x00,       // 0x0003: ld hl, sp+0  (save SP to HL)
        0x01, 0x34, 0x12, // 0x0005: ld bc, 0x1234
        0xc5,             // 0x0008: push bc (SP now 0xdfee)
        0xf9,             // 0x0009: ld sp, hl (restore SP to 0xdff0)
        0xc5,             // 0x000a: push bc again
        0xe1,             // 0x000b: pop hl (should get 0x1234)
        0x10              // 0x000c: stop
    };
    run_program(rom, 0);
    // If SP was correctly restored, the second push overwrote the first
    // and pop should give us 0x1234
    ASSERT_EQ(get_areg(REG_68K_A_HL) & 0xffff, 0x1234);
}

TEST(test_push_af)
{
    // push af, pop hl - transfer AF to HL via stack
    uint8_t rom[] = {
        0x3e, 0x42,       // 0x0000: ld a, 0x42
        0xaf,             // 0x0002: xor a (A=0, F=0x80 Z flag set)
        0x3e, 0xab,       // 0x0003: ld a, 0xab (F unchanged)
        0xf5,             // 0x0005: push af
        0xe1,             // 0x0006: pop hl
        0x10              // 0x0007: stop
    };
    run_program(rom, 0);
    // H = A = 0xab, L = F = 0x80
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xab);
    ASSERT_EQ(get_areg(REG_68K_A_HL) & 0xffff, 0xab80);
}

TEST(test_pop_af)
{
    // push hl, pop af - transfer HL to AF via stack
    uint8_t rom[] = {
        0x21, 0x90, 0xcd, // 0x0000: ld hl, 0xcd90 (A=0xcd, F=0x90)
        0xe5,             // 0x0003: push hl
        0xf1,             // 0x0004: pop af
        0x10              // 0x0005: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xcd);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0xff, 0x90);
}

TEST(test_ld_d_hl_ind)
{
    // ld d, (hl) - load byte at (HL) into D
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // 0x0000: ld hl, 0x5000
        0x36, 0xab,       // 0x0003: ld (hl), 0xab
        0x56,             // 0x0005: ld d, (hl)
        0x10              // 0x0006: stop
    };
    run_program(rom, 0);
    ASSERT_EQ((get_dreg(REG_68K_D_DE) >> 16) & 0xff, 0xab);
}

TEST(test_ld_e_hl_ind)
{
    // ld e, (hl) - load byte at (HL) into E
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // 0x0000: ld hl, 0x5000
        0x36, 0xcd,       // 0x0003: ld (hl), 0xcd
        0x5e,             // 0x0005: ld e, (hl)
        0x10              // 0x0006: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_DE) & 0xff, 0xcd);
}

TEST(test_ld_b_hl_ind)
{
    // ld b, (hl) - load byte at (HL) into B
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // 0x0000: ld hl, 0x5000
        0x36, 0x12,       // 0x0003: ld (hl), 0x12
        0x46,             // 0x0005: ld b, (hl)
        0x10              // 0x0006: stop
    };
    run_program(rom, 0);
    ASSERT_EQ((get_dreg(REG_68K_D_BC) >> 16) & 0xff, 0x12);
}

TEST(test_ld_c_hl_ind)
{
    // ld c, (hl) - load byte at (HL) into C
    uint8_t rom[] = {
        0x21, 0x00, 0x50, // 0x0000: ld hl, 0x5000
        0x36, 0x34,       // 0x0003: ld (hl), 0x34
        0x4e,             // 0x0005: ld c, (hl)
        0x10              // 0x0006: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x34);
}

// ld hl, $1000; ld bc, $0234; add hl, bc -> HL = 0x1234
TEST_EXEC(test_add_hl_bc,           REG_HL, 0x1234, 0x21, 0x00, 0x10, 0x01, 0x34, 0x02, 0x09, 0x10)

// ld b, $12; ld hl, $0000; ld h, b -> HL = 0x1200
TEST_EXEC(test_ld_h_b,              REG_HL, 0x1200, 0x06, 0x12, 0x21, 0x00, 0x00, 0x60, 0x10)

// ld c, $34; ld hl, $0000; ld l, c -> HL = 0x0034
TEST_EXEC(test_ld_l_c,              REG_HL, 0x0034, 0x0e, 0x34, 0x21, 0x00, 0x00, 0x69, 0x10)

TEST(test_rst_28)
{
    // rst 28h - call to address 0x0028
    uint8_t rom[0x100] = {0};
    // main code at 0x0000
    rom[0x00] = 0x3e; rom[0x01] = 0x11;  // ld a, 0x11
    rom[0x02] = 0xef;                     // rst 28h
    rom[0x03] = 0x3e; rom[0x04] = 0x33;  // ld a, 0x33 (after return)
    rom[0x05] = 0x10;                     // stop
    // handler at 0x0028
    rom[0x28] = 0x06; rom[0x29] = 0x22;  // ld b, 0x22
    rom[0x2a] = 0xc9;                     // ret
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x33);
    ASSERT_EQ((get_dreg(REG_68K_D_BC) >> 16) & 0xff, 0x22);
}

TEST(test_ldh_dec_ldh_loop)
{
    // Test the pattern: ldh a, (n); dec a; ldh (n), a; jr nz, loop
    // This is similar to the sprite counter decrement loop
    uint8_t rom[] = {
        0x3e, 0x03,       // 0x0000: ld a, 3
        0xe0, 0x90,       // 0x0002: ldh ($ff90), a  ; counter = 3
        // loop:
        0xf0, 0x90,       // 0x0004: ldh a, ($ff90)  ; read counter
        0x3d,             // 0x0006: dec a
        0xe0, 0x90,       // 0x0007: ldh ($ff90), a  ; write counter
        0x20, 0xf9,       // 0x0009: jr nz, -7 (back to 0x0004)
        0x10              // 0x000b: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x00);  // A should be 0
    ASSERT_EQ(get_mem_byte(0xff90), 0x00);          // counter should be 0
}

TEST(test_ldh_dec_preserves_value)
{
    // Verify that ldh read -> dec -> ldh write preserves the correct byte value
    // even if other registers have garbage in high bytes
    uint8_t rom[] = {
        0x3e, 0x05,       // 0x0000: ld a, 5
        0xe0, 0x91,       // 0x0002: ldh ($ff91), a  ; mem = 5
        0x3e, 0xff,       // 0x0004: ld a, 0xff      ; pollute A with 0xff
        0xf0, 0x91,       // 0x0006: ldh a, ($ff91)  ; read back (should be 5)
        0x3d,             // 0x0008: dec a           ; a = 4
        0xe0, 0x91,       // 0x0009: ldh ($ff91), a  ; write 4
        0x10              // 0x000b: stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x04);
    ASSERT_EQ(get_mem_byte(0x4011), 0x04);
}

// Slow path pop tests - SP set to address outside WRAM/HRAM (simulates Pokemon VBlankCopy)
// The slow path reads via dmg_read (stub_read in test harness) which reads from mem[].
// We use GB code (ld (hl), imm8) to write test data to mem before the pop.
TEST(test_pop_de_slow_path)
{
    // Write test data to $5000/$5001 using GB code, then pop from there
    uint8_t rom[] = {
        0x21, 0x00, 0x50,   // 0x0000: ld hl, $5000
        0x36, 0xab,         // 0x0003: ld (hl), $ab   ; low byte at $5000
        0x23,               // 0x0005: inc hl
        0x36, 0xcd,         // 0x0006: ld (hl), $cd   ; high byte at $5001
        0x31, 0x00, 0x50,   // 0x0008: ld sp, $5000   ; SP points to our data (slow mode)
        0xd1,               // 0x000b: pop de
        0x10                // 0x000c: stop
    };
    run_program(rom, 0);
    // DE in split format: 0x00DD00EE = 0x00cd00ab
    ASSERT_EQ(get_dreg(REG_68K_D_DE), 0x00cd00ab);
}

TEST(test_pop_bc_slow_path)
{
    uint8_t rom[] = {
        0x21, 0x00, 0x50,   // ld hl, $5000
        0x36, 0x34,         // ld (hl), $34  ; C
        0x23,               // inc hl
        0x36, 0x12,         // ld (hl), $12  ; B
        0x31, 0x00, 0x50,   // ld sp, $5000
        0xc1,               // pop bc
        0x10                // stop
    };
    run_program(rom, 0);
    // BC in split format: 0x00BB00CC = 0x00120034
    ASSERT_EQ(get_dreg(REG_68K_D_BC), 0x00120034);
}

TEST(test_pop_hl_slow_path)
{
    uint8_t rom[] = {
        0x21, 0x00, 0x50,   // ld hl, $5000
        0x36, 0x78,         // ld (hl), $78  ; L
        0x23,               // inc hl
        0x36, 0x56,         // ld (hl), $56  ; H
        0x31, 0x00, 0x50,   // ld sp, $5000
        0xe1,               // pop hl
        0x10                // stop
    };
    run_program(rom, 0);
    // HL is contiguous: 0xHHLL = 0x5678
    ASSERT_EQ(get_areg(REG_68K_A_HL) & 0xffff, 0x5678);
}

TEST(test_pop_af_slow_path)
{
    uint8_t rom[] = {
        0x21, 0x00, 0x50,   // ld hl, $5000
        0x36, 0x80,         // ld (hl), $80  ; F (Z flag set)
        0x23,               // inc hl
        0x36, 0xef,         // ld (hl), $ef  ; A
        0x31, 0x00, 0x50,   // ld sp, $5000
        0xf1,               // pop af
        0x10                // stop
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xef);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0xff, 0x80);
}

TEST(test_vblank_copy_pattern)
{
    // Simulates Pokemon's VBlankCopy: use pop to read from arbitrary memory
    uint8_t rom[] = {
        // Set up test data at $5000-$5003
        0x21, 0x00, 0x50,   // ld hl, $5000
        0x36, 0x11,         // ld (hl), $11
        0x23,               // inc hl
        0x36, 0x22,         // ld (hl), $22
        0x23,               // inc hl
        0x36, 0x33,         // ld (hl), $33
        0x23,               // inc hl
        0x36, 0x44,         // ld (hl), $44
        // Now do the VBlankCopy pattern
        0x31, 0x00, 0x50,   // ld sp, $5000
        0xd1,               // pop de -> DE = 0x00220011
        0x43,               // ld b, e (save E to B)
        0xd1,               // pop de -> DE = 0x00440033
        0x10                // stop
    };
    run_program(rom, 0);
    // After second pop, DE = 0x00440033
    ASSERT_EQ(get_dreg(REG_68K_D_DE), 0x00440033);
    // B should have first low byte 0x11 (from first pop's E value)
    ASSERT_EQ((get_dreg(REG_68K_D_BC) >> 16) & 0xff, 0x11);
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
    RUN_TEST(test_dec_b);
    RUN_TEST(test_dec_c);
    RUN_TEST(test_inc_c);
    RUN_TEST(test_inc_e);
    RUN_TEST(test_xor_a);

    printf("\n16-bit inc/dec:\n");
    RUN_TEST(test_dec_bc);

    printf("\nRegister-to-register loads:\n");
    RUN_TEST(test_ld_a_b);
    RUN_TEST(test_ld_b_a);
    RUN_TEST(test_ld_a_c);
    RUN_TEST(test_ld_c_a);
    RUN_TEST(test_ld_d_a);

    printf("\nMulti-block tests:\n");
    RUN_TEST(test_exec_jp_skip);
    RUN_TEST(test_exec_jr_forward);
    RUN_TEST(test_exec_jr_zero);
    RUN_TEST(test_exec_dec_a_loop);

    printf("\nFlags tests:\n");
    RUN_TEST(test_exec_cp_equal);
    RUN_TEST(test_exec_cp_not_equal);
    RUN_TEST(test_exec_cp_carry);
    RUN_TEST(test_exec_cp_hl_equal);
    RUN_TEST(test_exec_cp_hl_not_equal);
    RUN_TEST(test_exec_cp_hl_carry);

    printf("\nConditional jr tests:\n");
    RUN_TEST(test_exec_jr_z_taken);
    RUN_TEST(test_exec_jr_z_not_taken);
    RUN_TEST(test_exec_jr_c_taken);
    RUN_TEST(test_exec_jr_c_not_taken);
    RUN_TEST(test_exec_jr_nc_taken);
    RUN_TEST(test_exec_jr_nc_not_taken);

    printf("\nConditional jp tests:\n");
    RUN_TEST(test_exec_jp_z_taken);
    RUN_TEST(test_exec_jp_z_not_taken);
    RUN_TEST(test_exec_jp_nz_taken);
    RUN_TEST(test_exec_jp_nz_not_taken);
    RUN_TEST(test_exec_jp_c_taken);
    RUN_TEST(test_exec_jp_c_not_taken);
    RUN_TEST(test_exec_jp_nc_taken);
    RUN_TEST(test_exec_jp_nc_not_taken);

    printf("\nCall/ret tests:\n");
    RUN_TEST(test_exec_call_ret_simple);
    RUN_TEST(test_exec_call_ret_nested);
    RUN_TEST(test_exec_call_preserves_regs);

    printf("\nConditional ret tests:\n");
    RUN_TEST(test_exec_ret_nz_taken);
    RUN_TEST(test_exec_ret_nz_not_taken);
    RUN_TEST(test_exec_ret_z_taken);
    RUN_TEST(test_exec_ret_z_not_taken);
    RUN_TEST(test_exec_ret_nc_taken);
    RUN_TEST(test_exec_ret_nc_not_taken);
    RUN_TEST(test_exec_ret_c_taken);
    RUN_TEST(test_exec_ret_c_not_taken);

    printf("\nConditional call tests:\n");
    RUN_TEST(test_exec_call_nz_taken);
    RUN_TEST(test_exec_call_nz_not_taken);
    RUN_TEST(test_exec_call_z_taken);
    RUN_TEST(test_exec_call_z_not_taken);
    RUN_TEST(test_exec_call_nc_taken);
    RUN_TEST(test_exec_call_nc_not_taken);
    RUN_TEST(test_exec_call_c_taken);
    RUN_TEST(test_exec_call_c_not_taken);

    printf("\nMemory access tests:\n");
    RUN_TEST(test_exec_ld_bc_ind_a);
    RUN_TEST(test_exec_ld_a_bc_ind);
    RUN_TEST(test_exec_ld_de_ind_a);
    RUN_TEST(test_exec_ld_a_de_ind);
    RUN_TEST(test_exec_ld_hld_a);
    RUN_TEST(test_exec_ld_a_hli);
    RUN_TEST(test_exec_ldh_imm8_a);
    RUN_TEST(test_exec_ldh_c_a);
    RUN_TEST(test_exec_ldh_a_imm8);
    RUN_TEST(test_exec_ld_hl_ind_imm8);
    RUN_TEST(test_inc_hl_ind);
    RUN_TEST(test_dec_hl_ind);

    printf("\n8-bit ALU:\n");
    RUN_TEST(test_or_a_c);
    RUN_TEST(test_cpl);
    RUN_TEST(test_and_a_imm);
    RUN_TEST(test_swap_a);
    RUN_TEST(test_or_a_b);
    RUN_TEST(test_xor_a_c);
    RUN_TEST(test_and_a_c);
    RUN_TEST(test_and_a_a_nonzero);
    RUN_TEST(test_and_a_a_zero);
    RUN_TEST(test_exec_and_a_a_with_jr);
    RUN_TEST(test_add_a_a);
    RUN_TEST(test_add_a_d);
    RUN_TEST(test_add_adc_carry_propagation);
    RUN_TEST(test_ld_e_a);

    printf("\nADC/SBC tests:\n");
    RUN_TEST(test_adc_a_c_no_carry);
    RUN_TEST(test_adc_a_c_with_carry);
    RUN_TEST(test_adc_a_imm_with_carry);
    RUN_TEST(test_adc_overflow_sets_carry);
    RUN_TEST(test_adc_zero_flag);
    RUN_TEST(test_sbc_a_c_no_carry);
    RUN_TEST(test_sbc_a_c_with_carry);
    RUN_TEST(test_sbc_a_imm_with_carry);
    RUN_TEST(test_sbc_underflow_sets_carry);

    printf("\n16-bit ALU:\n");
    RUN_TEST(test_inc_l);
    RUN_TEST(test_dec_l);
    RUN_TEST(test_inc_hl);
    RUN_TEST(test_add_hl_de);

    printf("\nStack tests:\n");
    RUN_TEST(test_push_bc);
    RUN_TEST(test_push_de);
    RUN_TEST(test_push_hl);
    RUN_TEST(test_push_af);
    RUN_TEST(test_pop_de);
    RUN_TEST(test_pop_af);
    RUN_TEST(test_jp_hl);
    RUN_TEST(test_ld_hl_sp_plus_0);
    RUN_TEST(test_ld_hl_sp_plus_positive);
    RUN_TEST(test_ld_hl_sp_plus_negative);
    RUN_TEST(test_ld_sp_hl_roundtrip);

    printf("\nSlow path pop tests:\n");
    RUN_TEST(test_pop_de_slow_path);
    RUN_TEST(test_pop_bc_slow_path);
    RUN_TEST(test_pop_hl_slow_path);
    RUN_TEST(test_pop_af_slow_path);
    RUN_TEST(test_vblank_copy_pattern);

    printf("\nLoad from (HL):\n");
    RUN_TEST(test_ld_d_hl_ind);
    RUN_TEST(test_ld_e_hl_ind);
    RUN_TEST(test_ld_b_hl_ind);
    RUN_TEST(test_ld_c_hl_ind);

    printf("\nAdd HL tests:\n");
    RUN_TEST(test_add_hl_bc);

    printf("\nLD register tests:\n");
    RUN_TEST(test_ld_h_b);
    RUN_TEST(test_ld_l_c);

    printf("\nRST tests:\n");
    RUN_TEST(test_rst_28);

    printf("\nMisc tests:\n");
    RUN_TEST(test_ei);
    RUN_TEST(test_di);
    RUN_TEST(test_exec_jr_nz_then_call);

    printf("\nLDH counter tests:\n");
    RUN_TEST(test_ldh_dec_ldh_loop);
    RUN_TEST(test_ldh_dec_preserves_value);

    printf("\nCB prefix rotate/shift tests:\n");
    RUN_TEST(test_rlc_a);
    RUN_TEST(test_rlc_a_both_bits);
    RUN_TEST(test_rlc_b);
    RUN_TEST(test_rlc_c);
    RUN_TEST(test_rrc_a);
    RUN_TEST(test_rrc_a_both_bits);
    RUN_TEST(test_rrc_d);
    RUN_TEST(test_rrc_e);
    RUN_TEST(test_sla_a);
    RUN_TEST(test_sla_a_carry);
    RUN_TEST(test_sla_b);
    RUN_TEST(test_sra_a);
    RUN_TEST(test_sra_a_carry);
    RUN_TEST(test_sra_a_positive);
    RUN_TEST(test_sra_c);
    RUN_TEST(test_srl_a);
    RUN_TEST(test_srl_a_carry);
    RUN_TEST(test_srl_d);
    RUN_TEST(test_swap_b);
    RUN_TEST(test_swap_c);
    RUN_TEST(test_swap_d);
    RUN_TEST(test_swap_e);

    printf("\nCB prefix RL/RR tests:\n");
    RUN_TEST(test_rl_a_carry_in);
    RUN_TEST(test_rl_a_carry_out);
    RUN_TEST(test_rr_a_carry_in);
    RUN_TEST(test_rr_a_carry_out);

    printf("\nCB prefix H/L register tests:\n");
    RUN_TEST(test_swap_h);
    RUN_TEST(test_swap_l);
    RUN_TEST(test_rlc_h);
    RUN_TEST(test_rlc_l);
    RUN_TEST(test_srl_h);
    RUN_TEST(test_srl_l);

    printf("\nCB prefix (HL) indirect tests:\n");
    RUN_TEST(test_rlc_hl_ind);
    RUN_TEST(test_srl_hl_ind);
    RUN_TEST(test_swap_hl_ind);

    printf("\nCB prefix flag tests:\n");
    RUN_TEST(test_rlc_zero_flag);
    RUN_TEST(test_sla_carry_flag);
    RUN_TEST(test_swap_clears_carry);
}
