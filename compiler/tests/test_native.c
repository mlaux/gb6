/*
 * test_native.c - Native 68k Mac test driver
 *
 * Runs compiler tests directly on 68k hardware instead of using
 * the Musashi emulator. The generated code is executed natively
 * via function pointer casts.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../compiler.h"
#include "tests.h"

#define MEM_SIZE 0x10000
static uint8_t mem[MEM_SIZE];

#define CODE_BASE 0x1000
#define DEFAULT_GB_SP 0x0fff

#define HALT_SENTINEL 0xffffffff
#define MAX_CACHED_BLOCKS 256

/* Saved register state - persists between block executions */
static uint32_t dregs[8];
static uint32_t aregs[8];

/* Runtime context structure (must match JIT_CTX_* offsets in compiler.h) */
typedef struct {
    void *dmg;           /* offset 0 */
    void *read_func;     /* offset 4 */
    void *write_func;    /* offset 8 */
} jit_context;

static jit_context ctx;

/*
 * Assembly stubs that match the JIT calling convention.
 * JIT code pushes: value(2), addr(2), dmg(4), then JSR
 * So stack after jsr: ret(4), dmg(4), addr(2), value(2)
 *
 * These access the mem[] array using indexed addressing.
 */
asm(
    ".text\n"

    ".globl stub_write_asm\n"
    "stub_write_asm:\n"
    "    move.w 8(%sp), %d0\n"          /* d0 = gb addr */
    "    lea mem, %a0\n"               /* a0 = mem base */
    "    move.b 11(%sp), (%a0,%d0.w)\n" /* write value */
    "    rts\n"

    ".globl stub_read_asm\n"
    "stub_read_asm:\n"
    "    move.w 8(%sp), %d1\n"          /* d1 = gb addr */
    "    lea mem, %a0\n"               /* a0 = mem base */
    "    moveq #0, %d0\n"
    "    move.w 8(%sp), %d1\n"          /* d1 = addr again */
    "    move.b (%a0,%d1.w), %d0\n"     /* read value into d0 */
    "    rts\n"
);

extern void stub_write_asm(void);
extern uint32_t stub_read_asm(void);

static void setup_runtime(void)
{
    ctx.dmg = (void *)0x4000;  /* dummy, not used by our stubs */
    ctx.read_func = stub_read_asm;
    ctx.write_func = stub_write_asm;
}

/*
 * Execute a single compiled block.
 * Sets up registers, calls the code, reads registers back.
 */
 void execute_block(void *code)
{
    asm volatile(
        /* Save callee-saved registers */
        "movem.l %%d2-%%d7/%%a2-%%a4, -(%%sp)\n\t"

        /* Copy code pointer to A0 FIRST - GCC may have used A2-A4 */
        "movea.l %[code], %%a0\n\t"

        /* Load GB state into 68k registers */
        "move.l %[d4], %%d4\n\t"
        "move.l %[d5], %%d5\n\t"
        "move.l %[d6], %%d6\n\t"
        "move.l %[d7], %%d7\n\t"
        "movea.l %[a2], %%a2\n\t"
        "movea.l %[a3], %%a3\n\t"
        "movea.l %[a4], %%a4\n\t"

        /* Call the generated code */
        "jsr (%%a0)\n\t"

        /* Save results back to memory */
        "move.l %%d0, %[out_d0]\n\t"
        "move.l %%d4, %[out_d4]\n\t"
        "move.l %%d5, %[out_d5]\n\t"
        "move.l %%d6, %[out_d6]\n\t"
        "move.l %%d7, %[out_d7]\n\t"
        "move.l %%a2, %[out_a2]\n\t"
        "move.l %%a3, %[out_a3]\n\t"

        /* Restore callee-saved registers */
        "movem.l (%%sp)+, %%d2-%%d7/%%a2-%%a4\n\t"

        : [out_d0] "=m" (dregs[0]),
          [out_d4] "=m" (dregs[4]),
          [out_d5] "=m" (dregs[5]),
          [out_d6] "=m" (dregs[6]),
          [out_d7] "=m" (dregs[7]),
          [out_a2] "=m" (aregs[2]),
          [out_a3] "=m" (aregs[3])
        : [d4] "m" (dregs[4]),
          [d5] "m" (dregs[5]),
          [d6] "m" (dregs[6]),
          [d7] "m" (dregs[7]),
          [a2] "m" (aregs[2]),
          [a3] "m" (aregs[3]),
          [a4] "m" (aregs[4]),
          [code] "a" (code)
        : "d0", "d1", "d2", "d3", "a0", "a1", "cc", "memory"
    );
}

/*
 * Initialize state and run a single compiled block.
 * Used by TEST_EXEC macros for simple single-block tests.
 */
void run_code(struct code_block *block)
{
    int k;

    memset(mem, 0, MEM_SIZE);
    setup_runtime();

    /* Copy code to execution area */
    memcpy(mem + CODE_BASE, block->code, block->length);

    /* Clear all registers */
    for (k = 0; k < 8; k++) {
        dregs[k] = 0;
        aregs[k] = 0;
    }

    /* Initialize GB registers */
    aregs[2] = 0;                                /* HL = 0 (it's a value, not a pointer) */
    aregs[3] = (uint32_t)mem + DEFAULT_GB_SP;    /* SP = pointer into mem[] for call/ret */
    aregs[4] = &ctx;

    execute_block(mem + CODE_BASE);
}

/*
 * Run a complete GB program with block dispatcher.
 * Compiles and executes blocks until HALT_SENTINEL is returned.
 */
void run_program(uint8_t *gb_rom, uint16_t start_pc)
{
    struct code_block *cache[MAX_CACHED_BLOCKS] = {0};
    uint32_t pc = start_pc;
    int k;

    memset(mem, 0, MEM_SIZE);
    setup_runtime();

    /* Clear all registers */
    for (k = 0; k < 8; k++) {
        dregs[k] = 0;
        aregs[k] = 0;
    }

    /* Initialize GB registers */
    aregs[2] = 0;                                /* HL = 0 (it's a value, not a pointer) */
    aregs[3] = (uint32_t)mem + DEFAULT_GB_SP;    /* SP = pointer into mem[] for call/ret */
    aregs[4] = &ctx;

    while (1) {
        struct code_block *block = NULL;

        /* Look up or compile block */
        if (pc < MAX_CACHED_BLOCKS) {
            block = cache[pc];
        }
        if (!block) {
            block = compile_block(pc, gb_rom + pc);
            if (pc < MAX_CACHED_BLOCKS) {
                cache[pc] = block;
            }
        }

        /* Copy block code to execution area */
        memcpy(mem + CODE_BASE, block->code, block->length);

#ifdef DEBUG_COMPILE
        {
            uint8_t *p = mem + CODE_BASE;
            printf("  executing block at 0x%04x, code at %p, len=%d\n",
                   (unsigned)pc, (void *)p, (int)block->length);
            printf("  code bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                   p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
        }
#endif

        execute_block(mem + CODE_BASE);

#ifdef DEBUG_COMPILE
        printf("  returned, D0=0x%08lx\n", (unsigned long)dregs[0]);
#endif

        /* Check D0 for next PC or halt */
        pc = dregs[REG_68K_D_NEXT_PC];
        if (pc == HALT_SENTINEL) {
            break;
        }
    }

    /* Clean up cached blocks */
    for (k = 0; k < MAX_CACHED_BLOCKS; k++) {
        if (cache[k]) {
            block_free(cache[k]);
        }
    }
}

uint32_t get_dreg(int reg)
{
    return dregs[reg];
}

uint32_t get_areg(int reg)
{
    return aregs[reg];
}

uint8_t get_mem_byte(uint16_t addr)
{
    return mem[addr];
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("Native 68k compiler tests\n");
    compiler_init();

    register_unit_tests();
    register_exec_tests();

    printf("all tests passed\n");
    getchar();

    return 0;
}
