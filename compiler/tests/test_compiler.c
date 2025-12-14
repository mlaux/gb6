#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../compiler.h"
#include "tests.h"
#include "../musashi/m68k.h"

#define MEM_SIZE 0x10000
static uint8_t mem[MEM_SIZE];

#define CODE_BASE 0x1000
#define STACK_BASE 0x8000

// GB memory is mapped at base of 68k address space
// A1 = GB_MEM_BASE + GB_SP for stack operations
#define GB_MEM_BASE 0x0000
#define DEFAULT_GB_SP 0x0f00

int tests_run = 0;
int tests_passed = 0;

// Memory access callbacks for Musashi
unsigned int m68k_read_memory_8(unsigned int address)
{
    if (address < MEM_SIZE) {
        return mem[address];
    }
    printf("attempted to read byte from %08x\n", address);
    exit(1);
}

unsigned int m68k_read_memory_16(unsigned int address)
{
    if (address + 1 < MEM_SIZE) {
        return (mem[address] << 8) | mem[address + 1];
    }
    printf("attempted to read word from %08x\n", address);
    exit(1);
}

unsigned int m68k_read_memory_32(unsigned int address)
{
    if (address + 3 < MEM_SIZE) {
        return (mem[address] << 24) | (mem[address + 1] << 16) |
               (mem[address + 2] << 8) | mem[address + 3];
    }
    printf("attempted to read long from %08x\n", address);
    exit(1);
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    if (address < MEM_SIZE) {
        mem[address] = value & 0xff;
    } else {
        printf("attempted to write byte %08x = %d\n", address, value);
    }
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    if (address + 1 < MEM_SIZE) {
        mem[address] = (value >> 8) & 0xff;
        mem[address + 1] = value & 0xff;
    } else {
        printf("attempted to write word %08x = %d\n", address, value);
    }
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    if (address + 3 < MEM_SIZE) {
        mem[address] = (value >> 24) & 0xff;
        mem[address + 1] = (value >> 16) & 0xff;
        mem[address + 2] = (value >> 8) & 0xff;
        mem[address + 3] = value & 0xff;
    } else {
        printf("attempted to write long %08x = %d\n", address, value);
    }
}

// Initialize Musashi, copy code to memory, set up stack, run
void run_code(struct code_block *block)
{
    int k;

    memset(mem, 0, MEM_SIZE);

    // Copy code to CODE_BASE
    memcpy(mem + CODE_BASE, block->code, block->length);

    // each test case will return to address 0, which contains an infinite loop
    m68k_write_memory_32(STACK_BASE - 4, 0);
    m68k_write_memory_16(0, 0x60fe);  // bra.s *

    m68k_pulse_reset();

    m68k_set_reg(M68K_REG_SP, STACK_BASE - 4);
    m68k_set_reg(M68K_REG_ISP, STACK_BASE);
    m68k_set_reg(M68K_REG_PC, CODE_BASE);

    for (k = 0; k < 8; k++) {
        m68k_set_reg(M68K_REG_D0 + k, 0);
    }
    // Clear A0-A6, but not A7 (stack pointer)
    for (k = 0; k < 7; k++) {
        m68k_set_reg(M68K_REG_A0 + k, 0);
    }

    m68k_execute(1000);
}

#define HALT_SENTINEL 0xffffffff
#define MAX_CACHED_BLOCKS 256

// Run a complete GB program with block dispatcher
void run_program(uint8_t *gb_rom, uint16_t start_pc)
{
    struct code_block *cache[MAX_CACHED_BLOCKS] = {0};
    uint32_t pc = start_pc;
    int k;

    memset(mem, 0, MEM_SIZE);

    // Set up halt trap at address 0
    m68k_write_memory_16(0, 0x60fe);  // bra.s *

    m68k_pulse_reset();

    // Clear all registers once at start
    for (k = 0; k < 8; k++) {
        m68k_set_reg(M68K_REG_D0 + k, 0);
    }
    for (k = 0; k < 7; k++) {
        m68k_set_reg(M68K_REG_A0 + k, 0);
    }

    // Initialize GB stack pointer (A1 = base + SP)
    m68k_set_reg(M68K_REG_A1, GB_MEM_BASE + DEFAULT_GB_SP);

    while (1) {
        // Look up or compile block
        struct code_block *block = NULL;
        if (pc < MAX_CACHED_BLOCKS) {
            block = cache[pc];
        }
        if (!block) {
            block = compile_block(pc, gb_rom + pc);
            if (pc < MAX_CACHED_BLOCKS) {
                cache[pc] = block;
            }
        }

        // Copy block code to execution area
        memcpy(mem + CODE_BASE, block->code, block->length);

        // Set up return address to trap
        m68k_write_memory_32(STACK_BASE - 4, 0);
        m68k_set_reg(M68K_REG_SP, STACK_BASE - 4);
        m68k_set_reg(M68K_REG_PC, CODE_BASE);

        m68k_execute(1000);

        // Check D4 for next PC or halt
        pc = get_dreg(4);
        if (pc == HALT_SENTINEL) {
            break;
        }
    }

    // Clean up cached blocks
    for (k = 0; k < MAX_CACHED_BLOCKS; k++) {
        if (cache[k]) {
            block_free(cache[k]);
        }
    }
}

uint32_t get_dreg(int reg)
{
    return m68k_get_reg(NULL, M68K_REG_D0 + reg);
}

uint32_t get_areg(int reg)
{
    return m68k_get_reg(NULL, M68K_REG_A0 + reg);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("Initializing...\n");
    compiler_init();
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);

    register_unit_tests();
    register_exec_tests();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}
