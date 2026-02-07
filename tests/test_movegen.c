/*
 * Move generation tests - Perft validation
 * Perft counts all leaf nodes at a given depth, validating the entire
 * move generation + make/unmake pipeline.
 */

#include <stdio.h>
#include "../src/types.h"
#include "../src/board.h"
#include "../src/movegen.h"
#include "../src/tables.h"

extern int tests_run, tests_passed, tests_failed;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else { tests_failed++; printf("  FAIL: %s\n", msg); } \
} while(0)

/* Perft - enumerate all legal move paths to a given depth */
static u32 perft(u8 depth, u8 ply) {
    u32 nodes = 0;
    u16 num_moves, i, base_idx;

    if (depth == 0) return 1;

    num_moves = movegen_generate(ply);
    base_idx = g_state.move_buf_idx[ply];

    for (i = 0; i < num_moves; i++) {
        if (board_make_move(g_state.move_buf[base_idx + i])) {
            nodes += perft(depth - 1, ply + 1);
            board_unmake_move(g_state.move_buf[base_idx + i]);
        }
    }

    return nodes;
}

void test_movegen(void) {
    u32 nodes;
    char msg[80];

    /* Starting position perft values (well-known, universally agreed upon) */
    printf("  Starting position perft tests...\n");

    board_init();
    nodes = perft(1, 0);
    sprintf(msg, "Start perft(1) = %lu (expected 20)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 20, msg);

    board_init();
    nodes = perft(2, 0);
    sprintf(msg, "Start perft(2) = %lu (expected 400)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 400, msg);

    board_init();
    nodes = perft(3, 0);
    sprintf(msg, "Start perft(3) = %lu (expected 8902)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 8902, msg);

    board_init();
    printf("  Running perft(4) - this may take a moment...\n");
    nodes = perft(4, 0);
    sprintf(msg, "Start perft(4) = %lu (expected 197281)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 197281, msg);

    /* Kiwipete - complex position with many special moves */
    printf("  Kiwipete perft tests...\n");
    board_set_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

    nodes = perft(1, 0);
    sprintf(msg, "Kiwipete perft(1) = %lu (expected 48)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 48, msg);

    nodes = perft(2, 0);
    sprintf(msg, "Kiwipete perft(2) = %lu (expected 2039)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 2039, msg);

    nodes = perft(3, 0);
    sprintf(msg, "Kiwipete perft(3) = %lu (expected 97862)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 97862, msg);

    /* Position 3: en passant and promotion edge cases */
    printf("  Position 3 perft tests (en passant/promotion)...\n");
    board_set_fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");

    nodes = perft(1, 0);
    sprintf(msg, "Pos3 perft(1) = %lu (expected 14)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 14, msg);

    nodes = perft(2, 0);
    sprintf(msg, "Pos3 perft(2) = %lu (expected 191)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 191, msg);

    nodes = perft(3, 0);
    sprintf(msg, "Pos3 perft(3) = %lu (expected 2812)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 2812, msg);

    /* Position 4: castling edge cases */
    printf("  Position 4 perft tests (castling)...\n");
    board_set_fen("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");

    nodes = perft(1, 0);
    sprintf(msg, "Pos4 perft(1) = %lu (expected 6)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 6, msg);

    nodes = perft(2, 0);
    sprintf(msg, "Pos4 perft(2) = %lu (expected 264)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 264, msg);

    nodes = perft(3, 0);
    sprintf(msg, "Pos4 perft(3) = %lu (expected 9467)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 9467, msg);

    /* Position 5: another tricky position */
    printf("  Position 5 perft tests...\n");
    board_set_fen("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");

    nodes = perft(1, 0);
    sprintf(msg, "Pos5 perft(1) = %lu (expected 44)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 44, msg);

    nodes = perft(2, 0);
    sprintf(msg, "Pos5 perft(2) = %lu (expected 1486)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 1486, msg);

    nodes = perft(3, 0);
    sprintf(msg, "Pos5 perft(3) = %lu (expected 62379)", (unsigned long)nodes);
    TEST_ASSERT(nodes == 62379, msg);
}
