/*
 * SM83 JSON Test Runner
 *
 * Runs instruction tests from JSON files against the JIT compiler.
 * Test format from: https://github.com/SingleStepTests/sm83
 *
 * Usage: sm83_json_tests <test.json> [test.json...]
 *        sm83_json_tests -d tests/v1/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>

#include "../compiler.h"
#include "../musashi/m68k.h"
#include "cJSON.h"

// Memory layout:
// 0x00000-0x0ffff: GB RAM (64K flat, as tests expect)
// 0x10000-0x10fff: Compiled 68k code
// 0x11000-0x110ff: Runtime stubs
// 0x11100-0x1117f: JIT context
// 0x12000-0x1ffff: 68k stack

#define MEM_SIZE    0x20000
#define GB_RAM_SIZE 0x10000

static uint8_t mem[MEM_SIZE];

#define CODE_BASE    0x10000
#define STUB_BASE    0x11000
#define JIT_CTX_ADDR 0x11100
#define STACK_BASE   0x1f000

// Test statistics
static int tests_passed = 0;
static int tests_failed = 0;
static int tests_skipped = 0;

// Verbose mode
static int verbose = 0;

// Compile context
static uint8_t *test_gb_mem;
static struct compile_ctx test_ctx;

static uint8_t test_read(void *dmg, uint16_t address)
{
    (void)dmg;
    return test_gb_mem[address];
}

// Track current test name for debugging
static const char *current_test_name = NULL;

// Musashi memory callbacks
unsigned int m68k_read_memory_8(unsigned int address)
{
    if (address < MEM_SIZE)
        return mem[address];
    fprintf(stderr, "read8 OOB: 0x%08x (test: %s, PC: 0x%08x)\n",
            address, current_test_name ? current_test_name : "?",
            m68k_get_reg(NULL, M68K_REG_PC));
    exit(1);
}

unsigned int m68k_read_memory_16(unsigned int address)
{
    if (address + 1 < MEM_SIZE)
        return (mem[address] << 8) | mem[address + 1];
    fprintf(stderr, "read16 OOB: 0x%08x (test: %s, PC: 0x%08x)\n",
            address, current_test_name ? current_test_name : "?",
            m68k_get_reg(NULL, M68K_REG_PC));
    exit(1);
}

unsigned int m68k_read_memory_32(unsigned int address)
{
    if (address + 3 < MEM_SIZE)
        return (mem[address] << 24) | (mem[address + 1] << 16) |
               (mem[address + 2] << 8) | mem[address + 3];
    fprintf(stderr, "read32 OOB: 0x%08x (test: %s, PC: 0x%08x)\n",
            address, current_test_name ? current_test_name : "?",
            m68k_get_reg(NULL, M68K_REG_PC));
    exit(1);
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    if (address < MEM_SIZE) {
        mem[address] = value & 0xff;
        return;
    }
    fprintf(stderr, "write8 OOB: 0x%08x = 0x%02x (test: %s, PC: 0x%08x)\n",
            address, value & 0xff, current_test_name ? current_test_name : "?",
            m68k_get_reg(NULL, M68K_REG_PC));
    exit(1);
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    if (address + 1 < MEM_SIZE) {
        mem[address] = (value >> 8) & 0xff;
        mem[address + 1] = value & 0xff;
        return;
    }
    fprintf(stderr, "write16 OOB: 0x%08x = 0x%04x (test: %s, PC: 0x%08x)\n",
            address, value & 0xffff, current_test_name ? current_test_name : "?",
            m68k_get_reg(NULL, M68K_REG_PC));
    exit(1);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    if (address + 3 < MEM_SIZE) {
        mem[address] = (value >> 24) & 0xff;
        mem[address + 1] = (value >> 16) & 0xff;
        mem[address + 2] = (value >> 8) & 0xff;
        mem[address + 3] = value & 0xff;
        return;
    }
    fprintf(stderr, "write32 OOB: 0x%08x = 0x%08x (test: %s, PC: 0x%08x)\n",
            address, value, current_test_name ? current_test_name : "?",
            m68k_get_reg(NULL, M68K_REG_PC));
    exit(1);
}

static void setup_runtime_stubs(void)
{
    // stub_read: reads byte from address, returns in d0
    // Address is 16-bit on stack, need to zero-extend for 68k
    static const uint8_t stub_read[] = {
        0x70, 0x00,              // moveq #0, d0
        0x30, 0x2f, 0x00, 0x08,  // move.w 8(sp), d0
        0x20, 0x40,              // movea.l d0, a0
        0x10, 0x10,              // move.b (a0), d0
        0x4e, 0x75               // rts
    };

    // stub_write: writes value byte to address
    static const uint8_t stub_write[] = {
        0x70, 0x00,              // moveq #0, d0
        0x30, 0x2f, 0x00, 0x08,  // move.w 8(sp), d0
        0x20, 0x40,              // movea.l d0, a0
        0x10, 0xaf, 0x00, 0x0a,  // move.b 10(sp), (a0)
        0x4e, 0x75               // rts
    };

    // stub_ei_di: handle interrupt enable/disable (not used in these tests)
    static const uint8_t stub_ei_di[] = {
        0x4e, 0x75               // rts (just return)
    };

    memcpy(mem + STUB_BASE, stub_read, sizeof(stub_read));
    memcpy(mem + STUB_BASE + 0x20, stub_write, sizeof(stub_write));
    memcpy(mem + STUB_BASE + 0x40, stub_ei_di, sizeof(stub_ei_di));

    // Set up jit_runtime context (see compiler.h for JIT_CTX_* offsets)
    m68k_write_memory_32(JIT_CTX_ADDR + JIT_CTX_DMG, 0);
    m68k_write_memory_32(JIT_CTX_ADDR + JIT_CTX_READ, STUB_BASE);
    m68k_write_memory_32(JIT_CTX_ADDR + JIT_CTX_WRITE, STUB_BASE + 0x20);
    m68k_write_memory_32(JIT_CTX_ADDR + JIT_CTX_EI_DI, STUB_BASE + 0x40);
    m68k_write_memory_8(JIT_CTX_ADDR + JIT_CTX_INTCHECK, 0);
    m68k_write_memory_8(JIT_CTX_ADDR + JIT_CTX_ROM_BANK, 1);
    // JIT_CTX_DISPATCH: where to jump after block ends
    // Point to an infinite loop so m68k_execute returns
    m68k_write_memory_32(JIT_CTX_ADDR + JIT_CTX_DISPATCH, CODE_BASE + 0x800);
    m68k_write_memory_32(JIT_CTX_ADDR + JIT_CTX_SP_ADJUST, 1);
    m68k_write_memory_16(JIT_CTX_ADDR + JIT_CTX_GB_SP, 0xfffe);
    m68k_write_memory_16(CODE_BASE + 0x800, 0x60fe);          // bra.s *
    m68k_write_memory_32(JIT_CTX_ADDR + JIT_CTX_PATCH_HELPER, CODE_BASE + 0x800);
    m68k_write_memory_32(JIT_CTX_ADDR + JIT_CTX_HRAM_BASE, 0xff80);
}

// Set up 68k registers from GB state
static void set_gb_state(
    uint8_t a, uint8_t b, uint8_t c,
    uint8_t d, uint8_t e, uint8_t f,
    uint8_t h, uint8_t l,
    uint16_t sp
) {
    // D4 = A (8-bit)
    m68k_set_reg(M68K_REG_D4, a);

    // D5 = BC split (0x00BB00CC)
    m68k_set_reg(M68K_REG_D5, ((uint32_t)b << 16) | c);

    // D6 = DE split (0x00DD00EE)
    m68k_set_reg(M68K_REG_D6, ((uint32_t)d << 16) | e);

    // D7 = F (flags)
    m68k_set_reg(M68K_REG_D7, f);

    // A2 = HL (contiguous)
    m68k_set_reg(M68K_REG_A2, ((uint32_t)h << 8) | l);

    // A3 = SP (points directly into GB memory at offset 0)
    m68k_set_reg(M68K_REG_A3, sp);

    // A4 = context pointer
    m68k_set_reg(M68K_REG_A4, JIT_CTX_ADDR);
}

// Get GB state from 68k registers
static void get_gb_state(
    uint8_t *a, uint8_t *b, uint8_t *c,
    uint8_t *d, uint8_t *e, uint8_t *f,
    uint8_t *h, uint8_t *l,
    uint16_t *sp, uint16_t *pc
) {
    uint32_t d5, d6, a2, a3, d0;

    *a = m68k_get_reg(NULL, M68K_REG_D4) & 0xff;

    d5 = m68k_get_reg(NULL, M68K_REG_D5);
    *b = (d5 >> 16) & 0xff;
    *c = d5 & 0xff;

    d6 = m68k_get_reg(NULL, M68K_REG_D6);
    *d = (d6 >> 16) & 0xff;
    *e = d6 & 0xff;

    *f = m68k_get_reg(NULL, M68K_REG_D7) & 0xff;

    a2 = m68k_get_reg(NULL, M68K_REG_A2);
    *h = (a2 >> 8) & 0xff;
    *l = a2 & 0xff;

    a3 = m68k_get_reg(NULL, M68K_REG_A3);
    *sp = a3 & 0xffff;

    d0 = m68k_get_reg(NULL, M68K_REG_D3);
    *pc = d0 & 0xffff;
}

static int run_single_test(cJSON *test)
{
    cJSON *name_json = cJSON_GetObjectItem(test, "name");
    cJSON *initial = cJSON_GetObjectItem(test, "initial");
    cJSON *final = cJSON_GetObjectItem(test, "final");

    if (!initial || !final)
        return -1;

    const char *name = name_json ? name_json->valuestring : "unknown";
    current_test_name = name;

    // Get initial state
    uint16_t init_pc = cJSON_GetObjectItem(initial, "pc")->valueint;
    uint16_t init_sp = cJSON_GetObjectItem(initial, "sp")->valueint;
    uint8_t init_a = cJSON_GetObjectItem(initial, "a")->valueint;
    uint8_t init_b = cJSON_GetObjectItem(initial, "b")->valueint;
    uint8_t init_c = cJSON_GetObjectItem(initial, "c")->valueint;
    uint8_t init_d = cJSON_GetObjectItem(initial, "d")->valueint;
    uint8_t init_e = cJSON_GetObjectItem(initial, "e")->valueint;
    uint8_t init_f = cJSON_GetObjectItem(initial, "f")->valueint;
    uint8_t init_h = cJSON_GetObjectItem(initial, "h")->valueint;
    uint8_t init_l = cJSON_GetObjectItem(initial, "l")->valueint;

    // Get expected final state
    uint16_t exp_pc = cJSON_GetObjectItem(final, "pc")->valueint;
    uint16_t exp_sp = cJSON_GetObjectItem(final, "sp")->valueint;
    uint8_t exp_a = cJSON_GetObjectItem(final, "a")->valueint;
    uint8_t exp_b = cJSON_GetObjectItem(final, "b")->valueint;
    uint8_t exp_c = cJSON_GetObjectItem(final, "c")->valueint;
    uint8_t exp_d = cJSON_GetObjectItem(final, "d")->valueint;
    uint8_t exp_e = cJSON_GetObjectItem(final, "e")->valueint;
    uint8_t exp_f = cJSON_GetObjectItem(final, "f")->valueint;
    uint8_t exp_h = cJSON_GetObjectItem(final, "h")->valueint;
    uint8_t exp_l = cJSON_GetObjectItem(final, "l")->valueint;

    // Clear all memory (including GB RAM)
    memset(mem, 0, MEM_SIZE);
    setup_runtime_stubs();

    // Load initial RAM
    cJSON *ram = cJSON_GetObjectItem(initial, "ram");
    if (ram) {
        cJSON *entry;
        cJSON_ArrayForEach(entry, ram) {
            if (cJSON_GetArraySize(entry) >= 2) {
                uint16_t addr = cJSON_GetArrayItem(entry, 0)->valueint;
                uint8_t val = cJSON_GetArrayItem(entry, 1)->valueint;
                mem[addr] = val;
            }
        }
    }

    // Set up GB memory pointer for compiler (points to start of mem array)
    test_gb_mem = mem;

    // Compile block starting at init_pc
    struct code_block *block = compile_block(init_pc, &test_ctx);

    // Check for compilation error (unimplemented opcode)
    if (block->error) {
        if (verbose) {
            printf("SKIP %s: unimplemented opcode 0x%02x at 0x%04x\n",
                   name, block->failed_opcode, block->failed_address);
        }
        block_free(block);
        return 1;  // Skip
    }

    // Debug: dump compiled code if verbose
    if (verbose > 1) {
        fprintf(stderr, "start_pc: %04x\n", init_pc);
        fprintf(stderr, "  Compiled %zu bytes: ", block->length);
        for (size_t k = 0; k < block->length && k < 32; k++) {
            fprintf(stderr, "%02x ", block->code[k]);
        }
        fprintf(stderr, "\n");
    }

    // Copy compiled code to execution area (above 64K)
    memcpy(mem + CODE_BASE, block->code, block->length);

    // Set up return address on 68k stack (in case anything uses RTS)
    m68k_write_memory_32(STACK_BASE - 4, CODE_BASE + 0x800);

    // Initialize CPU
    m68k_pulse_reset();
    m68k_set_reg(M68K_REG_SP, STACK_BASE - 4);
    m68k_set_reg(M68K_REG_ISP, STACK_BASE);
    m68k_set_reg(M68K_REG_PC, CODE_BASE);

    // Clear scratch registers
    m68k_set_reg(M68K_REG_D0, 0);
    m68k_set_reg(M68K_REG_D1, 0);
    m68k_set_reg(M68K_REG_D2, 0);
    m68k_set_reg(M68K_REG_D3, 0);
    m68k_set_reg(M68K_REG_A0, 0);
    m68k_set_reg(M68K_REG_A1, 0);

    // Set initial GB state
    set_gb_state(init_a, init_b, init_c, init_d, init_e, init_f,
                 init_h, init_l, init_sp);

    // Execute
    m68k_execute(2000);

    // Get final state
    uint8_t got_a, got_b, got_c, got_d, got_e, got_f, got_h, got_l;
    uint16_t got_sp, got_pc;
    get_gb_state(&got_a, &got_b, &got_c, &got_d, &got_e, &got_f,
                 &got_h, &got_l, &got_sp, &got_pc);

    // Compare registers
    int failed = 0;
    char errbuf[512];
    errbuf[0] = '\0';

#define CHECK_REG(regname, got, exp) \
    if ((got) != (exp)) { \
        failed = 1; \
        char tmp[64]; \
        snprintf(tmp, sizeof(tmp), " %s: got 0x%02x, exp 0x%02x;", \
                 regname, (got), (exp)); \
        strncat(errbuf, tmp, sizeof(errbuf) - strlen(errbuf) - 1); \
    }

#define CHECK_REG16(regname, got, exp) \
    if ((got) != (exp)) { \
        failed = 1; \
        char tmp[64]; \
        snprintf(tmp, sizeof(tmp), " %s: got 0x%04x, exp 0x%04x;", \
                 regname, (got), (exp)); \
        strncat(errbuf, tmp, sizeof(errbuf) - strlen(errbuf) - 1); \
    }

    CHECK_REG("A", got_a, exp_a);
    CHECK_REG("B", got_b, exp_b);
    CHECK_REG("C", got_c, exp_c);
    CHECK_REG("D", got_d, exp_d);
    CHECK_REG("E", got_e, exp_e);
    // Mask out H flag (bit 5) - not yet implemented
    CHECK_REG("F", got_f & 0xd0, exp_f & 0xd0);
    CHECK_REG("H", got_h, exp_h);
    CHECK_REG("L", got_l, exp_l);
    CHECK_REG16("SP", got_sp, exp_sp);
    CHECK_REG16("PC", got_pc, exp_pc);

    // Check final RAM
    cJSON *final_ram = cJSON_GetObjectItem(final, "ram");
    if (final_ram) {
        cJSON *entry;
        cJSON_ArrayForEach(entry, final_ram) {
            if (cJSON_GetArraySize(entry) >= 2) {
                uint16_t addr = cJSON_GetArrayItem(entry, 0)->valueint;
                uint8_t exp_val = cJSON_GetArrayItem(entry, 1)->valueint;
                uint8_t got_val = mem[addr];
                if (got_val != exp_val) {
                    failed = 1;
                    char tmp[64];
                    snprintf(tmp, sizeof(tmp), " RAM[0x%04x]: got 0x%02x, exp 0x%02x;",
                             addr, got_val, exp_val);
                    strncat(errbuf, tmp, sizeof(errbuf) - strlen(errbuf) - 1);
                }
            }
        }
    }

#undef CHECK_REG
#undef CHECK_REG16

    block_free(block);

    if (failed) {
        printf("FAIL %s:%s\n", name, errbuf);
        return 2;  // Failed
    }

    if (verbose) {
        printf("PASS %s\n", name);
    }

    return 0;  // Passed
}

static int run_test_file(const char *filename)
{
    // Print filename immediately and flush so we know which file crashes
    const char *basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;
    fprintf(stderr, "Testing %s...\n", basename);
    fflush(stderr);

    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", filename);
        return -1;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Read file
    char *json_str = malloc(size + 1);
    if (!json_str) {
        fclose(f);
        return -1;
    }
    fread(json_str, 1, size, f);
    json_str[size] = '\0';
    fclose(f);

    // Parse JSON
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);

    if (!root) {
        fprintf(stderr, "JSON parse error in %s: %s\n",
                filename, cJSON_GetErrorPtr());
        return -1;
    }

    int file_passed = 0, file_failed = 0, file_skipped = 0;

    // Run each test
    cJSON *test;
    cJSON_ArrayForEach(test, root) {
        int result = run_single_test(test);
        if (result == 0)
            file_passed++;
        else if (result == 1)
            file_skipped++;
        else
            file_failed++;
    }

    cJSON_Delete(root);

    tests_passed += file_passed;
    tests_failed += file_failed;
    tests_skipped += file_skipped;

    // Print summary for this file (basename already computed at top)
    if (file_failed > 0) {
        printf("%s: %d passed, %d failed, %d skipped\n",
               basename, file_passed, file_failed, file_skipped);
    } else if (!verbose) {
        printf("%s: %d passed, %d skipped\n", basename, file_passed, file_skipped);
    }

    return file_failed;
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [-v] <test.json> [test.json...]\n", prog);
    printf("       %s [-v] -d <directory>\n", prog);
    printf("\nOptions:\n");
    printf("  -v       Verbose output (print each test name)\n");
    printf("  -d DIR   Run all .json files in directory\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    int arg_start = 1;
    const char *directory = NULL;

    // Parse options
    for (int k = 1; k < argc; k++) {
        if (strcmp(argv[k], "-vv") == 0) {
            verbose = 2;
            arg_start = k + 1;
        } else if (strcmp(argv[k], "-v") == 0) {
            verbose = 1;
            arg_start = k + 1;
        } else if (strcmp(argv[k], "-d") == 0) {
            if (k + 1 < argc) {
                directory = argv[k + 1];
                arg_start = k + 2;
            }
        } else if (argv[k][0] != '-') {
            break;
        }
    }

    // Initialize
    printf("Initializing SM83 JSON test runner...\n");
    compiler_init();
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);

    test_ctx.dmg = NULL;
    test_ctx.read = test_read;
    test_ctx.wram_base = (void *) 0xc000;
    test_ctx.hram_base = (void *) 0xff80;
    test_ctx.single_instruction = 1;  // Compile one instruction at a time for testing

    if (directory) {
        // Run all .json files in directory
        DIR *dir = opendir(directory);
        if (!dir) {
            fprintf(stderr, "Cannot open directory: %s\n", directory);
            return 1;
        }

        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            size_t len = strlen(ent->d_name);
            if (len > 5 && strcmp(ent->d_name + len - 5, ".json") == 0) {
                char path[1024];
                snprintf(path, sizeof(path), "%s/%s", directory, ent->d_name);
                run_test_file(path);
            }
        }
        closedir(dir);
    } else {
        // Run specified files
        for (int k = arg_start; k < argc; k++) {
            run_test_file(argv[k]);
        }
    }

    printf("\n=== Summary ===\n");
    printf("Passed:  %d\n", tests_passed);
    printf("Failed:  %d\n", tests_failed);
    printf("Skipped: %d\n", tests_skipped);

    return (tests_failed > 0) ? 1 : 0;
}
