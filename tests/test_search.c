/*
 * Search tests
 * - Mate-in-1 puzzles
 * - Mate-in-2 puzzles
 * - Tactical puzzles (win material)
 */

#include <stdio.h>
#include "../src/types.h"
#include "../src/board.h"
#include "../src/movegen.h"
#include "../src/search.h"
#include "../src/eval.h"
#include "../src/tt.h"
#include "../src/tables.h"

extern int tests_run, tests_passed, tests_failed;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else { tests_failed++; printf("  FAIL: %s\n", msg); } \
} while(0)

/* Check if the engine finds the expected move */
static u8 finds_move(const char *fen, u8 depth,
                      u8 exp_from, u8 exp_to) {
    SearchResult result;

    board_set_fen(fen);
    tt_clear();
    result = search_position(depth, 0);

    if (result.best_move.from == exp_from &&
        result.best_move.to == exp_to) {
        return 1;
    }

    /* Print what was found for debugging */
    {
        char from_str[3], to_str[3], exp_from_str[3], exp_to_str[3];
        sq_to_str(result.best_move.from, from_str);
        sq_to_str(result.best_move.to, to_str);
        sq_to_str(exp_from, exp_from_str);
        sq_to_str(exp_to, exp_to_str);
        printf("    Expected %s%s, got %s%s (score %d)\n",
               exp_from_str, exp_to_str, from_str, to_str, result.score);
    }

    return 0;
}

/* Check if the engine finds a mate score */
static u8 finds_mate(const char *fen, u8 depth) {
    SearchResult result;

    board_set_fen(fen);
    tt_clear();
    result = search_position(depth, 0);

    return IS_MATE_SCORE(result.score) && result.score > 0;
}

void test_search(void) {
    /* --- Mate in 1 puzzles --- */
    printf("  Mate-in-1 puzzles...\n");

    /* Scholar's mate: Qxf7# */
    TEST_ASSERT(
        finds_move("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4",
                    3, SQ_MAKE(4, 7), SQ_MAKE(6, 5)),
        "Mate in 1: Scholar's mate Qh5xf7#"
    );

    /* Back rank mate: Re8# */
    TEST_ASSERT(
        finds_move("6k1/5ppp/8/8/8/8/8/R3K3 w Q - 0 1",
                    3, SQ_MAKE(0, 0), SQ_MAKE(7, 0)),
        "Mate in 1: Back rank Ra1-a8#"
    );

    /* Queen delivers mate on h file: Qh5# (king boxed in by own pawns) */
    TEST_ASSERT(
        finds_mate("6k1/5ppp/8/8/8/5Q2/8/4K3 w - - 0 1", 4),
        "Mate in 1: finds mate with Q vs boxed king"
    );

    /* Simple rook mate: Ra8# with king on c6 boxing in king at a8 */
    TEST_ASSERT(
        finds_mate("k7/8/1K6/8/8/8/8/R7 w - - 0 1", 4),
        "Mate in 1: finds Ra1-a8# mating the king"
    );

    /* Knight + rook mate */
    TEST_ASSERT(
        finds_mate("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQ1RK1 b kq - 0 1",
                    2) == 0,
        "No mate in normal position (sanity check)"
    );

    /* --- Additional mate tests --- */
    printf("  Additional mate tests...\n");

    /* Engine should find forced mate in KRR vs K */
    TEST_ASSERT(
        finds_mate("k7/8/1K6/8/8/8/8/R6R w - - 0 1", 4),
        "Finds forced mate: two rooks with king support"
    );

    /* Engine recognizes it is being mated (negative mate score) */
    board_set_fen("k7/8/1K6/8/8/8/8/R6R b - - 0 1");
    tt_clear();
    {
        SearchResult res = search_position(4, 0);
        TEST_ASSERT(IS_MATE_SCORE(res.score) && res.score < 0,
            "Recognizes being mated (black to move, no escape)");
    }

    /* --- Evaluation sanity --- */
    printf("  Evaluation sanity tests...\n");

    /* Starting position should be roughly equal */
    board_init();
    {
        s16 score = eval_position();
        char msg[80];
        sprintf(msg, "Start position eval %d is near 0", score);
        TEST_ASSERT(score > -50 && score < 50, msg);
    }

    /* Position with extra queen should be highly positive */
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    /* Actually test a position where white has extra material */
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKQNR w KQkq - 0 1");
    /* That FEN might not parse cleanly. Use a simpler one: */
    board_set_fen("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1");
    {
        s16 score = eval_position();
        TEST_ASSERT(score > 800, "White with extra queen has high eval");
    }

    /* Black side should have negative eval mirrored */
    board_set_fen("4kq2/8/8/8/8/8/8/4K3 w - - 0 1");
    {
        s16 score = eval_position();
        TEST_ASSERT(score < -800, "Black with extra queen, white to move has low eval");
    }
}
