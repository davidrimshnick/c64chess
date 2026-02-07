/*
 * C64 Chess Engine - Test Harness
 * Runs board, movegen, and search tests.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/types.h"
#include "../src/board.h"
#include "../src/movegen.h"
#include "../src/search.h"
#include "../src/eval.h"
#include "../src/tt.h"
#include "../src/tables.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s\n", msg); } \
} while(0)

/* External test functions */
extern void test_board(void);
extern void test_movegen(void);
extern void test_search(void);

int main(void) {
    tables_init();

    printf("=== C64 Chess Engine Test Suite ===\n\n");

    printf("--- Board Tests ---\n");
    test_board();
    printf("\n");

    printf("--- Move Generation Tests ---\n");
    test_movegen();
    printf("\n");

    printf("--- Search Tests ---\n");
    test_search();
    printf("\n");

    printf("=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
