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
// Returns value of D0 after execution
uint32_t run_code(struct basic_block *block)
{
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
    m68k_set_reg(M68K_REG_D0, 0);

    int cycles = m68k_execute(1000);
    (void)cycles;

    return m68k_get_reg(NULL, M68K_REG_D0);
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
