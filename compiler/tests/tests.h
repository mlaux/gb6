#ifndef TESTS_H
#define TESTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../compiler.h"

extern int tests_run;
extern int tests_passed;

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

// Run compiled code on Musashi, return D0
uint32_t run_code(struct basic_block *block);

// Test registration functions
void register_unit_tests(void);
void register_exec_tests(void);

#endif
