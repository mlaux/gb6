#include "tests.h"

// ============================================================================
// HALT instruction tests
// HALT waits until vblank interrupt (LY 144, cycle 65664)
// Note: HALT's own 4 cycles are added to D2 before skip calculation,
// so true_pos = frame_cycles + 4
// ============================================================================

TEST(test_halt_before_vblank)
{
    // HALT when frame_cycles=0, true_pos=4, skip = 65664-4 = 65660
    uint8_t rom[] = {
        0x76              // halt
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_cycle_count(), 65664 - 4);
}

TEST(test_halt_mid_frame)
{
    // HALT at frame_cycles=10000, true_pos=10004, skip = 65664-10004 = 55660
    uint8_t rom[] = {
        0x76              // halt
    };
    run_block_with_frame_cycles(rom, 10000);
    ASSERT_EQ(get_cycle_count(), 65664 - 10000 - 4);
}

TEST(test_halt_just_before_vblank)
{
    // HALT at frame_cycles=65659, true_pos=65663, skip = 1
    uint8_t rom[] = {
        0x76              // halt
    };
    run_block_with_frame_cycles(rom, 65659);
    ASSERT_EQ(get_cycle_count(), 1);
}

TEST(test_halt_at_vblank_start)
{
    // HALT at frame_cycles=65660, true_pos=65664 (exactly at vblank)
    // In vblank path: skip = 135888 - 65664 = 70224
    uint8_t rom[] = {
        0x76              // halt
    };
    run_block_with_frame_cycles(rom, 65660);
    ASSERT_EQ(get_cycle_count(), 70224);
}

TEST(test_halt_during_vblank)
{
    // HALT at frame_cycles=68000, true_pos=68004 (in vblank)
    // skip = 135888 - 68004 = 67884
    uint8_t rom[] = {
        0x76              // halt
    };
    run_block_with_frame_cycles(rom, 68000);
    ASSERT_EQ(get_cycle_count(), 135888 - 68000 - 4);
}

TEST(test_halt_near_frame_end)
{
    // HALT at frame_cycles=70000, true_pos=70004 (near frame end)
    // skip = 135888 - 70004 = 65884
    uint8_t rom[] = {
        0x76              // halt
    };
    run_block_with_frame_cycles(rom, 70000);
    ASSERT_EQ(get_cycle_count(), 135888 - 70000 - 4);
}

// ============================================================================
// LY wait pattern tests
// Pattern: ldh a, [$44]; cp N; jr cc, back
// Compiler synthesizes a wait instead of spinning in a loop
// Note: The initial ld's cycles are added to D2
// before skip calculation, so true_pos = frame_cycles + 12
// ============================================================================

#define LY_WAIT_CYCLES 12

TEST(test_ly_wait_jr_nz_ly0)
{
    // ldh a, [$44]; cp 0; jr nz, back
    // Wait for LY=0 (frame start), from frame_cycles=0
    // true_pos = 20, target = 0, so wait until next frame
    // skip = (70224 + 0) - 20 = 70204
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44) - read LY
        0xfe, 0x00,       // cp 0
        0x20, 0xfa,       // jr nz, -6 (back to ldh)
        0x10              // stop (not reached, pattern is fused)
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0);
    ASSERT_EQ(get_cycle_count(), 70224 - LY_WAIT_CYCLES);
}

TEST(test_ly_wait_jr_nz_ly90)
{
    // ldh a, [$44]; cp 90; jr nz, back
    // Wait for LY=90, from frame_cycles=0
    // true_pos = 20, target = 90*456 = 41040
    // skip = 41040 - 20 = 41020
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44) - read LY
        0xfe, 0x5a,       // cp 90
        0x20, 0xfa,       // jr nz, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 90);
    ASSERT_EQ(get_cycle_count(), 90 * 456 - LY_WAIT_CYCLES);
}

TEST(test_ly_wait_jr_nz_ly144)
{
    // Wait for LY=144 (vblank start), from frame_cycles=0
    // true_pos = 20, target = 144*456 = 65664
    // skip = 65664 - 20 = 65644
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x90,       // cp 144
        0x20, 0xfa,       // jr nz, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 144);
    ASSERT_EQ(get_cycle_count(), 144 * 456 - LY_WAIT_CYCLES);
}

TEST(test_ly_wait_jr_nz_past_target)
{
    // Wait for LY=50, but frame_cycles already past that
    // frame_cycles=30000, true_pos=30020, target=22800
    // Since true_pos >= target, wait until next frame
    // skip = (70224 + 22800) - 30020 = 63004
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x32,       // cp 50
        0x20, 0xfa,       // jr nz, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 30000);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 50);
    ASSERT_EQ(get_cycle_count(), 70224 + (50 * 456) - 30000 - LY_WAIT_CYCLES);
}

TEST(test_ly_wait_jr_z_ly90)
{
    // ldh a, [$44]; cp 90; jr z, back
    // jr z: loop while LY == 90, exit when LY != 90
    // This waits for LY = (90 + 1) % 154 = 91
    // true_pos = 20, target = 91*456 = 41496
    // skip = 41496 - 20 = 41476
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x5a,       // cp 90
        0x28, 0xfa,       // jr z, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 91);
    ASSERT_EQ(get_cycle_count(), 91 * 456 - LY_WAIT_CYCLES);
}

TEST(test_ly_wait_jr_z_ly153)
{
    // ldh a, [$44]; cp 153; jr z, back
    // wait_ly = (153 + 1) % 154 = 0 (wraps to start of frame)
    // true_pos = 20, target = 0
    // Since true_pos >= target, wait for next frame
    // skip = (70224 + 0) - 20 = 70204
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x99,       // cp 153
        0x28, 0xfa,       // jr z, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 0);
    ASSERT_EQ(get_cycle_count(), 70224 - LY_WAIT_CYCLES);
}

TEST(test_ly_wait_jr_c_ly100)
{
    // ldh a, [$44]; cp 100; jr c, back
    // jr c: loop while LY < 100, exit when LY >= 100
    // true_pos = 20, target = 100*456 = 45600
    // skip = 45600 - 20 = 45580
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x64,       // cp 100
        0x38, 0xfa,       // jr c, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 0);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 100);
    ASSERT_EQ(get_cycle_count(), 100 * 456 - LY_WAIT_CYCLES);
}

TEST(test_ly_wait_mid_frame)
{
    // Wait for LY=100, starting at frame_cycles=20000
    // true_pos = 20020, target = 45600
    // skip = 45600 - 20020 = 25580
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x64,       // cp 100
        0x20, 0xfa,       // jr nz, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 20000);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 100);
    ASSERT_EQ(get_cycle_count(), 45600 - 20000 - LY_WAIT_CYCLES);
}

TEST(test_ly_wait_exact_target)
{
    // Start at frame_cycles such that true_pos exactly equals target
    // target = 50*456 = 22800, so frame_cycles = 22800 - 20 = 22780
    // true_pos = 22800 >= target, so wait for next frame
    // skip = (70224 + 22800) - 22800 = 70224
    uint8_t rom[] = {
        0xf0, 0x44,       // ldh a, ($ff44)
        0xfe, 0x32,       // cp 50
        0x20, 0xfa,       // jr nz, -6
        0x10              // stop
    };
    run_block_with_frame_cycles(rom, 22800 - LY_WAIT_CYCLES);
    ASSERT_EQ(get_dreg(REG_68K_D_A) & 0xff, 50);
    // true_pos == target_cycles, uses next frame path
    ASSERT_EQ(get_cycle_count(), 70224); 
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
}
