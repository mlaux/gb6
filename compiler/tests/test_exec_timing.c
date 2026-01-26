#include "tests.h"

// ============================================================================
// HALT instruction tests
// HALT waits until vblank interrupt (LY 144, cycle 65664)
// ============================================================================

TEST(test_halt_before_vblank)
{
    // HALT when frame_cycles=0 should wait 65664 cycles to reach vblank
    uint8_t rom[] = {
        0x76              // halt
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_cycle_count(), 65664);
}

TEST(test_halt_mid_frame)
{
    // HALT at frame_cycles=10000 should wait 55664 cycles (65664-10000)
    uint8_t rom[] = {
        0x76              // halt
    };
    run_block_with_frame_cycles(rom, 10000);
    ASSERT_EQ(get_cycle_count(), 65664 - 10000);
}

TEST(test_halt_just_before_vblank)
{
    // HALT at frame_cycles=65663 should wait 1 cycle
    uint8_t rom[] = {
        0x76              // halt
    };
    run_block_with_frame_cycles(rom, 65663);
    ASSERT_EQ(get_cycle_count(), 1);
}

TEST(test_halt_at_vblank_start)
{
    // HALT at exactly cycle 65664 (vblank start) should wait until next frame
    // cycles = (70224 + 65664) - 65664 = 70224
    uint8_t rom[] = {
        0x76              // halt
    };
    run_block_with_frame_cycles(rom, 65664);
    ASSERT_EQ(get_cycle_count(), 70224);
}

TEST(test_halt_during_vblank)
{
    // HALT at frame_cycles=68000 (in vblank) should wait until next frame vblank
    // cycles = (70224 + 65664) - 68000 = 135888 - 68000 = 67888
    uint8_t rom[] = {
        0x76              // halt
    };
    run_block_with_frame_cycles(rom, 68000);
    ASSERT_EQ(get_cycle_count(), 135888 - 68000);
}

TEST(test_halt_near_frame_end)
{
    // HALT at frame_cycles=70000 should wait until next frame vblank
    // cycles = (70224 + 65664) - 70000 = 65888
    uint8_t rom[] = {
        0x76              // halt
    };
    run_block_with_frame_cycles(rom, 70000);
    ASSERT_EQ(get_cycle_count(), 135888 - 70000);
}

// ============================================================================
// LY wait pattern tests
// Pattern: ldh a, [$44]; cp N; jr cc, back
// Compiler synthesizes a wait instead of spinning in a loop
// ============================================================================

TEST(test_ly_wait_jr_nz_ly0)
{
    // ldh a, [$44]; cp 0; jr nz, back
    // Wait for LY=0 (frame start), from frame_cycles=0
    // target_cycles = 0 * 456 = 0, so wait until next frame
    // D2 = (70224 + 0) - 0 = 70224, A = 0
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44) - read LY
        0xfe, 0x00,       // cp 0
        0x20, 0xfa,       // jr nz, -6 (back to ldh)
        0x10              // stop (not reached, pattern is fused)
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0);
    // At frame_cycles=0, waiting for LY 0 means next frame
    ASSERT_EQ(get_cycle_count(), 70224);
}

TEST(test_ly_wait_jr_nz_ly90)
{
    // ldh a, [$44]; cp 90; jr nz, back
    // Wait for LY=90, from frame_cycles=0
    // target_cycles = 90 * 456 = 41040
    // D2 = 41040 - 0 = 41040, A = 90
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44) - read LY
        0xfe, 0x5a,       // cp 90
        0x20, 0xfa,       // jr nz, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 90);
    ASSERT_EQ(get_cycle_count(), 90 * 456);
}

TEST(test_ly_wait_jr_nz_ly144)
{
    // Wait for LY=144 (vblank start), from frame_cycles=0
    // target_cycles = 144 * 456 = 65664
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x90,       // cp 144
        0x20, 0xfa,       // jr nz, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 144);
    ASSERT_EQ(get_cycle_count(), 144 * 456);
}

TEST(test_ly_wait_jr_nz_past_target)
{
    // Wait for LY=50, but frame_cycles already past that
    // frame_cycles=30000, LY 50 is at 22800
    // Since frame_cycles >= target, wait until next frame
    // D2 = (70224 + 22800) - 30000 = 63024
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x32,       // cp 50
        0x20, 0xfa,       // jr nz, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 30000);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 50);
    ASSERT_EQ(get_cycle_count(), 70224 + (50 * 456) - 30000);
}

TEST(test_ly_wait_jr_z_ly90)
{
    // ldh a, [$44]; cp 90; jr z, back
    // jr z: loop while LY == 90, exit when LY != 90
    // This waits for LY = (90 + 1) % 154 = 91
    // target_cycles = 91 * 456 = 41496
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x5a,       // cp 90
        0x28, 0xfa,       // jr z, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 91);
    ASSERT_EQ(get_cycle_count(), 91 * 456);
}

TEST(test_ly_wait_jr_z_ly153)
{
    // ldh a, [$44]; cp 153; jr z, back
    // wait_ly = (153 + 1) % 154 = 0 (wraps to start of frame)
    // target_cycles = 0 * 456 = 0
    // From frame_cycles=0, this should wait for next frame
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x99,       // cp 153
        0x28, 0xfa,       // jr z, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0);
    // At frame_cycles=0, target is 0, so next frame
    ASSERT_EQ(get_cycle_count(), 70224);
}

TEST(test_ly_wait_jr_c_ly100)
{
    // ldh a, [$44]; cp 100; jr c, back
    // jr c: loop while LY < 100, exit when LY >= 100
    // wait_ly = 100, target_cycles = 100 * 456 = 45600
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x64,       // cp 100
        0x38, 0xfa,       // jr c, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 100);
    ASSERT_EQ(get_cycle_count(), 100 * 456);
}

TEST(test_ly_wait_mid_frame)
{
    // Wait for LY=100, starting at frame_cycles=20000
    // LY 100 is at cycle 45600
    // D2 = 45600 - 20000 = 25600
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x64,       // cp 100
        0x20, 0xfa,       // jr nz, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 20000);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 100);
    ASSERT_EQ(get_cycle_count(), 45600 - 20000);
}

TEST(test_ly_wait_exact_target)
{
    // Start exactly at the target LY cycle
    // LY 50 is at cycle 22800, start there
    // frame_cycles >= target_cycles, so wait for next frame
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x32,       // cp 50
        0x20, 0xfa,       // jr nz, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 22800);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 50);
    // frame_cycles == target_cycles, uses next frame path
    ASSERT_EQ(get_cycle_count(), 70224);
}

// ============================================================================
// LY wait pattern tests with register compare
// Pattern: ld r, N; ldh a, [$44]; cp r; jr cc, back
// Same as immediate but target comes from a register
// ============================================================================

TEST(test_ly_wait_reg_cp_b)
{
    // ld b, 90; ldh a, [$44]; cp b; jr nz, back
    // Wait for LY=90
    uint8_t rom[] = {
        0x06, 0x5a,       // ld b, 90
        0xf0, 0x44,       // ldh a, ($ff44)
        0xb8,             // cp b
        0x20, 0xfb,       // jr nz, -5 (back to ldh)
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 90);
    ASSERT_EQ(get_cycle_count(), 90 * 456);
}

TEST(test_ly_wait_reg_cp_c)
{
    // ld c, 100; ldh a, [$44]; cp c; jr nz, back
    // Wait for LY=100
    uint8_t rom[] = {
        0x0e, 0x64,       // ld c, 100
        0xf0, 0x44,       // ldh a, ($ff44)
        0xb9,             // cp c
        0x20, 0xfb,       // jr nz, -5
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 100);
    ASSERT_EQ(get_cycle_count(), 100 * 456);
}

TEST(test_ly_wait_reg_cp_h)
{
    // ld h, 144; ldh a, [$44]; cp h; jr nz, back
    // Wait for LY=144 (vblank)
    uint8_t rom[] = {
        0x26, 0x90,       // ld h, 144
        0xf0, 0x44,       // ldh a, ($ff44)
        0xbc,             // cp h
        0x20, 0xfb,       // jr nz, -5
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 144);
    ASSERT_EQ(get_cycle_count(), 144 * 456);
}

TEST(test_ly_wait_reg_cp_l)
{
    // ld l, 50; ldh a, [$44]; cp l; jr nz, back
    // Wait for LY=50
    uint8_t rom[] = {
        0x2e, 0x32,       // ld l, 50
        0xf0, 0x44,       // ldh a, ($ff44)
        0xbd,             // cp l
        0x20, 0xfb,       // jr nz, -5
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 50);
    ASSERT_EQ(get_cycle_count(), 50 * 456);
}

TEST(test_ly_wait_reg_jr_z)
{
    // ld b, 90; ldh a, [$44]; cp b; jr z, back
    // jr z: loop while LY == 90, wait for LY = 91
    uint8_t rom[] = {
        0x06, 0x5a,       // ld b, 90
        0xf0, 0x44,       // ldh a, ($ff44)
        0xb8,             // cp b
        0x28, 0xfb,       // jr z, -5
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 91);
    ASSERT_EQ(get_cycle_count(), 91 * 456);
}

TEST(test_ly_wait_reg_jr_z_wrap)
{
    // ld c, 153; ldh a, [$44]; cp c; jr z, back
    // jr z with LY=153: wait_ly = (153+1) % 154 = 0
    uint8_t rom[] = {
        0x0e, 0x99,       // ld c, 153
        0xf0, 0x44,       // ldh a, ($ff44)
        0xb9,             // cp c
        0x28, 0xfb,       // jr z, -5
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0);
    // target=0, true_pos > 0, so wait for next frame
    ASSERT_EQ(get_cycle_count(), 70224);
}

TEST(test_ly_wait_reg_jr_c)
{
    // ld d, 100; ldh a, [$44]; cp d; jr c, back
    // jr c: loop while LY < 100, wait for LY >= 100
    uint8_t rom[] = {
        0x16, 0x64,       // ld d, 100
        0xf0, 0x44,       // ldh a, ($ff44)
        0xba,             // cp d
        0x38, 0xfb,       // jr c, -5
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 100);
    ASSERT_EQ(get_cycle_count(), 100 * 456);
}

TEST(test_ly_wait_reg_mid_frame)
{
    // Wait for LY=100, starting at frame_cycles=20000
    uint8_t rom[] = {
        0x06, 0x64,       // ld b, 100
        0xf0, 0x44,       // ldh a, ($ff44)
        0xb8,             // cp b
        0x20, 0xfb,       // jr nz, -5
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 20000);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 100);
    ASSERT_EQ(get_cycle_count(), 100 * 456 - 20000);
}

TEST(test_ly_wait_reg_past_target)
{
    // Wait for LY=50, but frame_cycles already past that
    // target = 50*456 = 22800, frame_cycles = 30000
    // Wait until next frame
    uint8_t rom[] = {
        0x1e, 0x32,       // ld e, 50
        0xf0, 0x44,       // ldh a, ($ff44)
        0xbb,             // cp e
        0x20, 0xfb,       // jr nz, -5
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 30000);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 50);
    ASSERT_EQ(get_cycle_count(), 70224 + 50 * 456 - 30000);
}

void register_timing_tests(void)
{
    printf("\nHALT instruction tests:\n");
    RUN_TEST(test_halt_before_vblank);
    RUN_TEST(test_halt_mid_frame);
    RUN_TEST(test_halt_just_before_vblank);
    RUN_TEST(test_halt_at_vblank_start);
    RUN_TEST(test_halt_during_vblank);
    RUN_TEST(test_halt_near_frame_end);

    printf("\nLY wait pattern tests:\n");
    RUN_TEST(test_ly_wait_jr_nz_ly0);
    RUN_TEST(test_ly_wait_jr_nz_ly90);
    RUN_TEST(test_ly_wait_jr_nz_ly144);
    RUN_TEST(test_ly_wait_jr_nz_past_target);
    RUN_TEST(test_ly_wait_jr_z_ly90);
    RUN_TEST(test_ly_wait_jr_z_ly153);
    RUN_TEST(test_ly_wait_jr_c_ly100);
    RUN_TEST(test_ly_wait_mid_frame);
    RUN_TEST(test_ly_wait_exact_target);

    printf("\nLY wait register pattern tests:\n");
    RUN_TEST(test_ly_wait_reg_cp_b);
    RUN_TEST(test_ly_wait_reg_cp_c);
    RUN_TEST(test_ly_wait_reg_cp_h);
    RUN_TEST(test_ly_wait_reg_cp_l);
    RUN_TEST(test_ly_wait_reg_jr_z);
    RUN_TEST(test_ly_wait_reg_jr_z_wrap);
    RUN_TEST(test_ly_wait_reg_jr_c);
    RUN_TEST(test_ly_wait_reg_mid_frame);
    RUN_TEST(test_ly_wait_reg_past_target);
}
