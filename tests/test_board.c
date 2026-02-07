/*
 * Board representation tests
 * - FEN parsing
 * - Make/unmake move
 * - Zobrist hash consistency
 * - Attack detection
 */

#include <stdio.h>
#include <string.h>
#include "../src/types.h"
#include "../src/board.h"
#include "../src/tables.h"
#include "../src/tt.h"

extern int tests_run, tests_passed, tests_failed;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else { tests_failed++; printf("  FAIL: %s\n", msg); } \
} while(0)

void test_board(void) {
    char fen_buf[100];

    /* Test 1: Starting position FEN round-trip */
    board_init();
    board_get_fen(fen_buf);
    TEST_ASSERT(
        strncmp(fen_buf, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
                54) == 0,
        "Starting position FEN round-trip"
    );

    /* Test 2: Piece placement */
    board_init();
    TEST_ASSERT(g_state.board[SQ_E1] == W_KING, "White king on e1");
    TEST_ASSERT(g_state.board[SQ_E8] == B_KING, "Black king on e8");
    TEST_ASSERT(g_state.board[SQ_A1] == W_ROOK, "White rook on a1");
    TEST_ASSERT(g_state.board[SQ_D8] == B_QUEEN, "Black queen on d8");
    TEST_ASSERT(g_state.board[SQ_MAKE(1, 4)] == W_PAWN, "White pawn on e2");

    /* Test 3: King positions tracked */
    TEST_ASSERT(g_state.king_sq[WHITE] == SQ_E1, "White king tracked at e1");
    TEST_ASSERT(g_state.king_sq[BLACK] == SQ_E8, "Black king tracked at e8");

    /* Test 4: Side to move */
    TEST_ASSERT(g_state.side == WHITE, "White to move in start position");

    /* Test 5: Castling rights */
    TEST_ASSERT(g_state.castle_rights == CASTLE_ALL, "All castling rights in start pos");

    /* Test 6: FEN parsing - complex position */
    board_set_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    TEST_ASSERT(g_state.board[SQ_MAKE(4, 3)] == W_PAWN, "Kiwipete: pawn on d5");
    TEST_ASSERT(g_state.board[SQ_MAKE(4, 4)] == W_KNIGHT, "Kiwipete: knight on e5");
    TEST_ASSERT(g_state.castle_rights == CASTLE_ALL, "Kiwipete: all castling rights");

    /* Test 7: Zobrist hash consistency */
    board_init();
    {
        u16 hash1 = g_state.hash;
        u16 hash2 = board_compute_hash();
        TEST_ASSERT(hash1 == hash2, "Zobrist hash matches computed hash");
    }

    /* Test 8: Make/unmake preserves hash */
    board_init();
    {
        u16 hash_before = g_state.hash;
        Move m;
        m.from = SQ_MAKE(1, 4); /* e2 */
        m.to = SQ_MAKE(3, 4);   /* e4 */
        m.flags = MF_PAWNSTART;
        m.score = 0;

        if (board_make_move(m)) {
            board_unmake_move(m);
            TEST_ASSERT(g_state.hash == hash_before,
                "Hash restored after make/unmake e2e4");
        } else {
            TEST_ASSERT(0, "e2e4 should be legal");
        }
    }

    /* Test 9: Make/unmake preserves board state */
    board_init();
    {
        u8 board_copy[128];
        u8 castle_before = g_state.castle_rights;
        u8 ep_before = g_state.ep_square;
        Move m;

        memcpy(board_copy, g_state.board, 128);

        m.from = SQ_MAKE(0, 6); /* g1 */
        m.to = SQ_MAKE(2, 5);   /* f3 */
        m.flags = MF_NONE;
        m.score = 0;

        if (board_make_move(m)) {
            board_unmake_move(m);
            TEST_ASSERT(memcmp(g_state.board, board_copy, 128) == 0,
                "Board restored after make/unmake Nf3");
            TEST_ASSERT(g_state.castle_rights == castle_before,
                "Castle rights restored after make/unmake");
            TEST_ASSERT(g_state.ep_square == ep_before,
                "EP square restored after make/unmake");
        }
    }

    /* Test 10: Attack detection */
    board_init();
    TEST_ASSERT(board_is_square_attacked(SQ_MAKE(2, 4), WHITE) == 1,
        "e3 attacked by white pawn");
    TEST_ASSERT(board_is_square_attacked(SQ_MAKE(2, 5), WHITE) == 1,
        "f3 attacked by white knight (g1)");
    TEST_ASSERT(board_is_square_attacked(SQ_MAKE(4, 4), WHITE) == 0,
        "e5 not attacked by white in start pos");

    /* Test 11: EP square set after double pawn push */
    board_init();
    {
        Move m;
        m.from = SQ_MAKE(1, 4); /* e2 */
        m.to = SQ_MAKE(3, 4);   /* e4 */
        m.flags = MF_PAWNSTART;
        m.score = 0;

        board_make_move(m);
        TEST_ASSERT(g_state.ep_square == SQ_MAKE(2, 4),
            "EP square set to e3 after e2e4");
    }

    /* Test 12: Material counting */
    board_init();
    {
        /* White material: K(20000) + Q(900) + 2R(1000) + 2B(660) + 2N(640) + 8P(800) = 24000 */
        s16 expected = 20000 + 900 + 1000 + 660 + 640 + 800;
        TEST_ASSERT(g_state.material[WHITE] == expected,
            "White material correct in start pos");
        TEST_ASSERT(g_state.material[BLACK] == expected,
            "Black material correct in start pos");
    }
}
