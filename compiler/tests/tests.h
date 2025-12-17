#ifndef TESTS_H
#define TESTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../compiler.h"

// Register encoding:
//   bits 0-3: register number (0-7)
//   bit 4: 0 = data reg, 1 = address reg
//   bits 8-15: mask type (0 = 8-bit, 1 = split 32-bit, 2 = 16-bit)
#define REG_A   0x0004  // D4, 8-bit
#define REG_BC  0x0105  // D5, split (0x00BB00CC)
#define REG_DE  0x0106  // D6, split (0x00DD00EE)
#define REG_HL  0x0212  // A2, 16-bit contiguous (0xHHLL)
#define REG_SP  0x0213  // A3, 16-bit contiguous

#define REG_INDEX(r)   ((r) & 0x0f)
#define REG_IS_ADDR(r) ((r) & 0x10)
#define REG_MASK(r)    (((r) & 0x0200) ? 0xffff : (((r) & 0x0100) ? 0x00ff00ff : 0xff))

#define TEST(name) static void name(void)
#define RUN_TEST(name) do { \
    printf("  %s... ", #name); \
    name(); \
    printf("ok\n"); \
} while (0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL\n    %s:%d: expected 0x%x, got 0x%x\n", \
               __FILE__, __LINE__, (unsigned)(b), (unsigned)(a)); \
        exit(1); \
    } \
} while (0)

#define ASSERT_BYTES(block, ...) do { \
    uint8_t expected[] = { __VA_ARGS__ }; \
    size_t n = sizeof(expected); \
    ASSERT_EQ((block)->length, n); \
    for (size_t k = 0; k < n; k++) { \
        if ((block)->code[k] != expected[k]) { \
            printf("FAIL\n    %s:%d: byte %zu: expected 0x%02x, got 0x%02x\n", \
                   __FILE__, __LINE__, k, expected[k], (block)->code[k]); \
            exit(1); \
        } \
    } \
} while (0)

#define TEST_EXEC(name, reg, expected, ...) \
    TEST(name) { \
        uint8_t gb_code[] = { __VA_ARGS__ }; \
        struct code_block *block = compile_block(0, gb_code); \
        run_code(block); \
        uint32_t raw = REG_IS_ADDR(reg) ? get_areg(REG_INDEX(reg)) : get_dreg(REG_INDEX(reg)); \
        uint32_t result = raw & REG_MASK(reg); \
        ASSERT_EQ(result, expected); \
        block_free(block); \
    }


// Run compiled code on Musashi
void run_code(struct code_block *block);

// Run a complete GB program with block dispatcher
void run_program(uint8_t *gb_rom, uint16_t start_pc);

// Get 68k register values
uint32_t get_dreg(int reg);
uint32_t get_areg(int reg);

// Get simulated memory byte
uint8_t get_mem_byte(uint16_t addr);

// Test registration functions
void register_unit_tests(void);
void register_exec_tests(void);

#endif
