#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../compiler.h"
#include "../musashi/m68k.h"

// Simple memory for the 68k emulator
#define MEM_SIZE 0x10000
static uint8_t mem[MEM_SIZE];

// Code gets loaded here
#define CODE_BASE 0x1000

// Stack starts here (grows down)
#define STACK_BASE 0x8000

// Memory access callbacks for Musashi
unsigned int m68k_read_memory_8(unsigned int address)
{
    if (address < MEM_SIZE) {
        return mem[address];
    }
    return 0;
}

unsigned int m68k_read_memory_16(unsigned int address)
{
    if (address + 1 < MEM_SIZE) {
        return (mem[address] << 8) | mem[address + 1];
    }
    return 0;
}

unsigned int m68k_read_memory_32(unsigned int address)
{
    if (address + 3 < MEM_SIZE) {
        return (mem[address] << 24) | (mem[address + 1] << 16) |
               (mem[address + 2] << 8) | mem[address + 3];
    }
    return 0;
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    if (address < MEM_SIZE) {
        mem[address] = value & 0xff;
    }
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    if (address + 1 < MEM_SIZE) {
        mem[address] = (value >> 8) & 0xff;
        mem[address + 1] = value & 0xff;
    }
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    if (address + 3 < MEM_SIZE) {
        mem[address] = (value >> 24) & 0xff;
        mem[address + 1] = (value >> 16) & 0xff;
        mem[address + 2] = (value >> 8) & 0xff;
        mem[address + 3] = value & 0xff;
    }
}

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name) do { \
    printf("  %s... ", #name); \
    tests_run++; \
    name(); \
    tests_passed++; \
    printf("ok\n"); \
} while (0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL\n    %s:%d: expected %d, got %d\n", \
               __FILE__, __LINE__, (int)(b), (int)(a)); \
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

// Initialize Musashi, copy code to memory, set up stack, run
// Returns value of D0 after execution
static uint32_t run_code(struct basic_block *block)
{
    memset(mem, 0, MEM_SIZE);

    // Copy code to CODE_BASE
    memcpy(mem + CODE_BASE, block->code, block->length);

    // Set up reset vector (not really needed since we set PC directly)
    // Initial SP at 0x0000, Initial PC at 0x0004
    m68k_write_memory_32(0, STACK_BASE);
    m68k_write_memory_32(4, CODE_BASE);

    // Push a return address onto the stack so rts has somewhere to go
    // We'll use address 0 as a sentinel (contains illegal instruction)
    m68k_write_memory_16(0, 0x4afc);  // illegal instruction at 0
    m68k_write_memory_32(STACK_BASE - 4, 0);  // return address = 0

    m68k_set_reg(M68K_REG_SP, STACK_BASE - 4);
    m68k_set_reg(M68K_REG_PC, CODE_BASE);

    // Clear D0
    m68k_set_reg(M68K_REG_D0, 0);

    // Run until we hit the illegal instruction or enough cycles pass
    int cycles = m68k_execute(1000);
    (void)cycles;

    return m68k_get_reg(NULL, M68K_REG_D0);
}

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

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("Initializing...\n");
    compiler_init();
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);

    printf("\nUnit tests (byte output):\n");
    RUN_TEST(test_nop_ret);
    RUN_TEST(test_ld_a_imm8);
    RUN_TEST(test_ld_a_zero);
    RUN_TEST(test_ld_a_ff);

    printf("\nExecution tests (Musashi):\n");
    RUN_TEST(test_exec_ld_a_imm8);
    RUN_TEST(test_exec_ld_a_zero);
    RUN_TEST(test_exec_ld_a_ff);
    RUN_TEST(test_exec_multiple_ld_a);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}
