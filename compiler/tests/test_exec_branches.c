#include "tests.h"

// JP instruction
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

// JR instructions
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x00);
}

// CP tests (comparison - sets flags)
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
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x04, 0x04);  // Z flag set
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
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x04, 0x00);  // Z flag clear
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
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 1, 1);  // C flag set
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x04, 0x04);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 0x04, 0x00);
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
    ASSERT_EQ(get_dreg(REG_68K_D_FLAGS) & 1, 1);
}

// Conditional JR tests
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x99);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x10);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x99);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x99);
}

// Conditional JP tests
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x99);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x99);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x88);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x77);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x10);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x55);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x66);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x42);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x44);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);
}

// JP (HL)
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

// Call/ret tests
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x33);
    ASSERT_EQ((get_dreg(REG_68K_D_BC) >> 16) & 0xff, 0x22);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0x04);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);
    ASSERT_EQ((get_dreg(REG_68K_D_BC) >> 16) & 0xff, 0xbb);
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0xcc);
}

// Conditional ret tests
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);
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
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x99);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xbb);
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
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x77);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xcc);
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
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x55);
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
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xdd);
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
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x66);
}

// Conditional call tests
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
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x77);
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
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x00);
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
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x88);
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
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x00);
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
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x99);
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
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x00);
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
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x55);
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
    ASSERT_EQ(get_dreg(REG_68K_D_BC) & 0xff, 0x00);
}

// RST instruction
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

// Chained ret tests (Pokemon TryDoWildEncounter pattern)
TEST(test_chained_ret_nz_both_return)
{
    // Test the actual Pokemon bug: ret nz -> ret nz
    // Outer calls middle, middle calls inner
    // Inner does: and a; ret nz (with A != 0, so returns)
    // Middle does: ret nz (should also return because Z was 0)
    // Outer should get control back and set A = 0xaa
    uint8_t rom[] = {
        0x3e, 0x42,        // 0x00: ld a, 0x42 (nonzero)
        0xcd, 0x20, 0x00,  // 0x02: call middle (0x0020)
        0x3e, 0xaa,        // 0x05: ld a, 0xaa (should execute after both rets)
        0x10, 0x00,        // 0x07: stop
        // padding to 0x10
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // inner function at 0x10
        0xa7,              // 0x10: and a (Z=0 because A=0x42)
        0xc0,              // 0x11: ret nz (returns because Z=0)
        0x3e, 0xdd,        // 0x12: ld a, 0xdd (should NOT execute)
        0xc9,              // 0x14: ret
        // padding to 0x20
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // middle function at 0x20
        0xcd, 0x10, 0x00,  // 0x20: call inner (0x0010)
        0xc0,              // 0x23: ret nz (should return because Z=0 from inner)
        0x3e, 0xcc,        // 0x24: ld a, 0xcc (should NOT execute)
        0xc9,              // 0x26: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);
}

TEST(test_chained_ret_nz_inner_falls_through)
{
    // Inner does: and a; ret nz (with A == 0, so falls through)
    // Inner then sets A = 0x42 and returns normally
    // Middle does: ret nz (should return because now Z=0)
    uint8_t rom[] = {
        0x3e, 0x00,        // 0x00: ld a, 0x00 (zero)
        0xcd, 0x20, 0x00,  // 0x02: call middle (0x0020)
        0x3e, 0xaa,        // 0x05: ld a, 0xaa (should execute after middle returns)
        0x10, 0x00,        // 0x07: stop
        // padding to 0x10
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // inner function at 0x10
        0xa7,              // 0x10: and a (Z=1 because A=0)
        0xc0,              // 0x11: ret nz (does NOT return because Z=1)
        0x3e, 0x42,        // 0x12: ld a, 0x42 (executes, sets A nonzero)
        0xa7,              // 0x14: and a (Z=0 now)
        0xc9,              // 0x15: ret
        // padding to 0x20
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // middle function at 0x20
        0xcd, 0x10, 0x00,  // 0x20: call inner (0x0010)
        0xc0,              // 0x23: ret nz (should return because Z=0 from inner's and a)
        0x3e, 0xcc,        // 0x24: ld a, 0xcc (should NOT execute)
        0xc9,              // 0x26: ret
    };
    run_program(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);
}

TEST(test_chained_ret_nz_neither_returns)
{
    // Inner does: and a; ret nz (with A == 0, so falls through)
    // Inner returns with Z=1 (from xor a)
    // Middle does: ret nz (should NOT return because Z=1)
    // Middle falls through and sets A = 0xcc
    uint8_t rom[] = {
        0x3e, 0x00,        // 0x00: ld a, 0x00 (zero)
        0xcd, 0x20, 0x00,  // 0x02: call middle (0x0020)
        0x3e, 0xaa,        // 0x05: ld a, 0xaa
        0x10, 0x00,        // 0x07: stop
        // padding to 0x10
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // inner function at 0x10
        0xa7,              // 0x10: and a (Z=1 because A=0)
        0xc0,              // 0x11: ret nz (does NOT return because Z=1)
        0xaf,              // 0x12: xor a (A=0, Z=1)
        0xc9,              // 0x13: ret
        // padding to 0x20
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // middle function at 0x20
        0xcd, 0x10, 0x00,  // 0x20: call inner (0x0010)
        0xc0,              // 0x23: ret nz (should NOT return because Z=1)
        0x3e, 0xcc,        // 0x24: ld a, 0xcc (should execute)
        0xc9,              // 0x26: ret
    };
    run_program(rom, 0);
    // Middle's ret nz should NOT return, so A = 0xcc, then outer sets A = 0xaa
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0xaa);
}

// Interrupt enable/disable
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

void register_branch_tests(void)
{
    printf("\nJP instruction:\n");
    RUN_TEST(test_exec_jp_skip);

    printf("\nJR instructions:\n");
    RUN_TEST(test_exec_jr_forward);
    RUN_TEST(test_exec_jr_zero);
    RUN_TEST(test_exec_dec_a_loop);

    printf("\nCP (comparison) tests:\n");
    RUN_TEST(test_exec_cp_equal);
    RUN_TEST(test_exec_cp_not_equal);
    RUN_TEST(test_exec_cp_carry);
    RUN_TEST(test_exec_cp_hl_equal);
    RUN_TEST(test_exec_cp_hl_not_equal);
    RUN_TEST(test_exec_cp_hl_carry);

    printf("\nConditional JR tests:\n");
    RUN_TEST(test_exec_jr_z_taken);
    RUN_TEST(test_exec_jr_z_not_taken);
    RUN_TEST(test_exec_jr_nz_then_call);
    RUN_TEST(test_exec_jr_c_taken);
    RUN_TEST(test_exec_jr_c_not_taken);
    RUN_TEST(test_exec_jr_nc_taken);
    RUN_TEST(test_exec_jr_nc_not_taken);

    printf("\nConditional JP tests:\n");
    RUN_TEST(test_exec_jp_z_taken);
    RUN_TEST(test_exec_jp_z_not_taken);
    RUN_TEST(test_exec_jp_nz_taken);
    RUN_TEST(test_exec_jp_nz_not_taken);
    RUN_TEST(test_exec_jp_c_taken);
    RUN_TEST(test_exec_jp_c_not_taken);
    RUN_TEST(test_exec_jp_nc_taken);
    RUN_TEST(test_exec_jp_nc_not_taken);

    printf("\nJP (HL):\n");
    RUN_TEST(test_jp_hl);

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

    printf("\nRST tests:\n");
    RUN_TEST(test_rst_28);

    printf("\nChained ret preserves flags:\n");
    RUN_TEST(test_chained_ret_nz_both_return);
    RUN_TEST(test_chained_ret_nz_inner_falls_through);
    RUN_TEST(test_chained_ret_nz_neither_returns);

    printf("\nInterrupt enable/disable:\n");
    RUN_TEST(test_ei);
    RUN_TEST(test_di);
}
