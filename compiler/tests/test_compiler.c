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
#define STUB_BASE 0x2000   // Where stub functions live
#define JIT_CTX_ADDR 0x3000 // jit_runtime context structure
#define STACK_BASE 0x8000

// GB memory is mapped at base of 68k address space
#define GB_MEM_BASE 0x0000
#define DEFAULT_GB_SP 0x0fff

// Test compile context
uint8_t *test_gb_rom;
static struct compile_ctx test_ctx;
struct compile_ctx *test_compile_ctx = &test_ctx;

// Read function for test compiler context
static uint8_t test_read(void *dmg, uint16_t address)
{
    (void)dmg;
    return test_gb_rom[address];
}

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

// Set up stub functions for dmg_read/dmg_write
// These are 68k code that the compiled JIT code can call
// Use A0 as scratch
static void setup_runtime_stubs(void)
{
    // stub_write: writes value byte to address
    // Stack layout after jsr: ret(4), dmg(4), addr(2), val(2)
    // Note: must zero-extend address since movea.w sign-extends
    // moveq #0, d0        ; clear d0
    // move.w 8(sp), d0    ; d0 = address (zero-extended)
    // movea.l d0, a0      ; a0 = address
    // move.b 11(sp), (a0) ; write value to memory
    // rts
    static const uint8_t stub_write[] = {
        0x70, 0x00,              // moveq #0, d0
        0x30, 0x2f, 0x00, 0x08,  // move.w 8(sp), d0
        0x20, 0x40,              // movea.l d0, a0
        0x10, 0xaf, 0x00, 0x0b,  // move.b 11(sp), (a0)
        0x4e, 0x75               // rts
    };

    // stub_read: reads byte from address, returns in d0
    // Stack layout after jsr: ret(4), dmg(4), addr(2)
    // Note: must zero-extend address since movea.w sign-extends
    // moveq #0, d0        ; clear d0
    // move.w 8(sp), d0    ; d0 = address (zero-extended)
    // movea.l d0, a0      ; a0 = address
    // move.b (a0), d0     ; read value
    // rts
    static const uint8_t stub_read[] = {
        0x70, 0x00,              // moveq #0, d0
        0x30, 0x2f, 0x00, 0x08,  // move.w 8(sp), d0
        0x20, 0x40,              // movea.l d0, a0
        0x10, 0x10,              // move.b (a0), d0
        0x4e, 0x75               // rts
    };

    static const uint8_t stub_ei_di[] = {
        0x11, 0xef, 0x00, 0x09, 0x40, 0x00, // move.b 9(sp), (U8_INTERRUPTS_ENABLED)
        0x4e, 0x75                          // rts
    };

    // Copy stubs to memory
    memcpy(mem + STUB_BASE, stub_read, sizeof(stub_read));
    memcpy(mem + STUB_BASE + 0x20, stub_write, sizeof(stub_write));
    memcpy(mem + STUB_BASE + 0x40, stub_ei_di, sizeof(stub_ei_di));

    // Set up jit_runtime context structure at JIT_CTX_ADDR
    // offset 0: dmg pointer (just use a non-null dummy)
    // offset 4: read function pointer
    // offset 8: write function pointer
    // 12: enable/disable interrupts pointer
    m68k_write_memory_32(JIT_CTX_ADDR + 0, 0x00004000);  // dmg = some address
    m68k_write_memory_32(JIT_CTX_ADDR + 4, STUB_BASE);
    m68k_write_memory_32(JIT_CTX_ADDR + 8, STUB_BASE + 0x20);
    m68k_write_memory_32(JIT_CTX_ADDR + 12, STUB_BASE + 0x40);
}

// Initialize Musashi, copy code to memory, set up stack, run
void run_code(struct code_block *block)
{
    int k;

    memset(mem, 0, MEM_SIZE);

    // Set up runtime stubs and context
    setup_runtime_stubs();

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

    // Set A4 to runtime context
    m68k_set_reg(M68K_REG_A4, JIT_CTX_ADDR);

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

    // Set up runtime stubs and context
    setup_runtime_stubs();

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

    // Initialize GB stack pointer (A3 = base + SP)
    m68k_set_reg(M68K_REG_A3, GB_MEM_BASE + DEFAULT_GB_SP);

    // Set A4 to runtime context
    m68k_set_reg(M68K_REG_A4, JIT_CTX_ADDR);

    while (1) {
        // Look up or compile block
        struct code_block *block = NULL;
        if (pc < MAX_CACHED_BLOCKS) {
            block = cache[pc];
        }
        if (!block) {
            test_gb_rom = gb_rom;
            block = compile_block(pc, test_compile_ctx);
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

        // Check D0 for next PC or halt
        pc = get_dreg(REG_68K_D_NEXT_PC);
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

uint8_t get_mem_byte(uint16_t addr)
{
    return mem[addr];
}

void set_mem_byte(uint16_t addr, uint8_t value)
{
    mem[addr] = value;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("Initializing...\n");
    compiler_init();
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);

    // Initialize test compile context
    test_ctx.dmg = NULL;
    test_ctx.read = test_read;
    test_ctx.wram_base = NULL;
    test_ctx.hram_base = NULL;

    register_unit_tests();
    register_exec_tests();

    printf("all tests passed\n");

    return 0;
}
