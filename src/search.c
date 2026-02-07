#include "search.h"
#include "board.h"
#include "movegen.h"
#include "eval.h"
#include "movesort.h"
#include "tt.h"
#include "tables.h"

#ifdef TARGET_C64
#include <c64.h>
#else
#include <stdio.h>
#endif

#ifndef TARGET_C64
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
#endif

SearchInfo g_search_info;

/* Triangular PV table */
static Move pv_table[MAX_PLY][MAX_PLY];
static u8 pv_length[MAX_PLY];

/* --- Platform-specific timing --- */

u32 get_time_ms(void) {
#ifdef TARGET_C64
    /* C64 jiffy clock at $A0-$A2, ticks at 60Hz */
    u32 jiffies;
    jiffies = (u32)(*(u8 *)0xA0) << 16;
    jiffies |= (u32)(*(u8 *)0xA1) << 8;
    jiffies |= (u32)(*(u8 *)0xA2);
    /* Convert jiffies to ms: ms = jiffies * 1000 / 60 */
    return (jiffies * 17); /* approximate: 1000/60 ~ 16.67 */
#elif defined(_WIN32)
    return GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u32)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

void search_check_time(void) {
    if (!g_search_info.use_time) return;

    /* Only check every 1024 nodes to reduce overhead */
    if ((g_search_info.nodes & 1023) != 0) return;

    if (get_time_ms() - g_search_info.start_time >= g_search_info.max_time_ms) {
        g_search_info.stopped = 1;
    }
}

/* --- Quiescence Search --- */

static s16 quiescence(s16 alpha, s16 beta, u8 ply) {
    s16 stand_pat;
    u16 num_moves, i, base_idx;

    if (g_search_info.stopped) return 0;
    g_search_info.nodes++;

    /* Stand-pat: use static eval as lower bound */
    stand_pat = eval_position();
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    /* Generate capture moves only */
    num_moves = movegen_generate_captures(ply);
    if (num_moves == 0) return alpha;

    /* Score and sort captures */
    movesort_score_moves(ply, num_moves, NULL);
    base_idx = g_state.move_buf_idx[ply];

    for (i = 0; i < num_moves; i++) {
        movesort_pick_best(ply, i, num_moves);

        if (!board_make_move(g_state.move_buf[base_idx + i])) continue;

        {
            s16 score = -quiescence(-beta, -alpha, ply + 1);
            board_unmake_move(g_state.move_buf[base_idx + i]);

            if (g_search_info.stopped) return 0;
            if (score > alpha) {
                alpha = score;
                if (score >= beta) return beta;
            }
        }
    }

    return alpha;
}

/* --- Negamax with Alpha-Beta --- */

static s16 negamax(s16 alpha, s16 beta, u8 depth, u8 ply, u8 do_null) {
    u16 num_moves, i, base_idx;
    u16 legal_moves = 0;
    s16 best_score = -SCORE_INFINITY;
    Move best_move;
    Move pv_move;
    u8 tt_flag = TT_FLAG_ALPHA;
    u8 in_check;
    u8 has_pv = 0;
    s16 score;

    best_move.from = 0;
    best_move.to = 0;
    best_move.flags = 0;
    best_move.score = 0;

    pv_length[ply] = ply;

    if (g_search_info.stopped) return 0;

    /* Check for draw by repetition or fifty-move rule */
    if (ply > 0 && (board_is_repetition() || g_state.fifty_clock >= 100)) {
        return SCORE_DRAW;
    }

    /* TT probe */
    {
        s16 tt_score;
        Move tt_move;
        tt_move.from = 0; tt_move.to = 0; tt_move.flags = 0; tt_move.score = 0;

        if (ply > 0 && tt_probe(g_state.hash, depth, alpha, beta,
                                 &tt_score, &tt_move, ply)) {
            return tt_score;
        }
        /* Even if no score cutoff, we may have a best move for ordering */
        if (tt_move.from != 0 || tt_move.to != 0) {
            pv_move = tt_move;
            has_pv = 1;
        }
    }

    /* Leaf node: quiescence search */
    if (depth == 0) {
        return quiescence(alpha, beta, ply);
    }

    g_search_info.nodes++;
    search_check_time();
    if (g_search_info.stopped) return 0;

    in_check = board_in_check();

    /* Check extension: search one deeper when in check */
    if (in_check) {
        depth++;
    }

    /* Null Move Pruning:
     * If we can give the opponent a free move and still get a beta cutoff,
     * this position is probably too good to bother searching fully.
     * Skip when: in check, at low depth, or in endgame (zugzwang risk). */
    if (do_null && !in_check && depth >= 3 && !eval_is_endgame()) {
        u8 R = 3; /* reduction */
        if (depth > 6) R = 4;

        board_make_null();
        score = -negamax(-beta, -beta + 1, depth - 1 - R, ply + 1, 0);
        board_unmake_null();

        if (g_search_info.stopped) return 0;
        if (score >= beta) {
            return beta;
        }
    }

    /* Generate moves */
    num_moves = movegen_generate(ply);
    base_idx = g_state.move_buf_idx[ply];

    /* Score moves for ordering */
    movesort_score_moves(ply, num_moves, has_pv ? &pv_move : NULL);

    /* Search all moves */
    for (i = 0; i < num_moves; i++) {
        movesort_pick_best(ply, i, num_moves);

        if (!board_make_move(g_state.move_buf[base_idx + i])) continue;
        legal_moves++;

        /* Late Move Reductions (LMR):
         * After searching a few moves fully, reduce depth for later quiet moves.
         * They're unlikely to be best. If reduced search surprises us, re-search. */
        if (legal_moves > 4 && depth >= 3 && !in_check &&
            !(g_state.move_buf[base_idx + i].flags & (MF_CAPTURE | MF_PROMO))) {
            /* Reduced depth search */
            score = -negamax(-alpha - 1, -alpha, depth - 2, ply + 1, 1);
            if (score > alpha) {
                /* Re-search at full depth if LMR found something */
                score = -negamax(-beta, -alpha, depth - 1, ply + 1, 1);
            }
        } else {
            score = -negamax(-beta, -alpha, depth - 1, ply + 1, 1);
        }

        board_unmake_move(g_state.move_buf[base_idx + i]);

        if (g_search_info.stopped) return 0;

        if (score > best_score) {
            best_score = score;
            best_move = g_state.move_buf[base_idx + i];

            if (score > alpha) {
                alpha = score;
                tt_flag = TT_FLAG_EXACT;

                /* Update PV */
                pv_table[ply][ply] = g_state.move_buf[base_idx + i];
                {
                    u8 j;
                    for (j = ply + 1; j < pv_length[ply + 1]; j++) {
                        pv_table[ply][j] = pv_table[ply + 1][j];
                    }
                    pv_length[ply] = pv_length[ply + 1];
                }

                if (score >= beta) {
                    /* Beta cutoff - update killers */
                    movesort_update_killers(ply, g_state.move_buf[base_idx + i]);
                    tt_store(g_state.hash, depth, beta, TT_FLAG_BETA,
                             best_move, ply);
                    return beta;
                }
            }
        }
    }

    /* No legal moves: checkmate or stalemate */
    if (legal_moves == 0) {
        if (in_check) {
            return -SCORE_MATE + ply; /* checkmate */
        }
        return SCORE_DRAW; /* stalemate */
    }

    /* Store in TT */
    tt_store(g_state.hash, depth, best_score, tt_flag, best_move, ply);

    return best_score;
}

/* --- Iterative Deepening --- */

SearchResult search_position(u8 max_depth, u32 max_time_ms) {
    SearchResult result;
    u8 depth;
    s16 score;
    Move best_move_so_far;

    result.best_move.from = 0;
    result.best_move.to = 0;
    result.best_move.flags = 0;
    result.best_move.score = 0;
    result.score = 0;
    result.depth = 0;
    result.nodes = 0;

    best_move_so_far.from = 0;
    best_move_so_far.to = 0;
    best_move_so_far.flags = 0;
    best_move_so_far.score = 0;

    /* Set up move buffer for ply 0 */
    g_state.move_buf_idx[0] = 0;

    /* Initialize search info */
    g_search_info.nodes = 0;
    g_search_info.max_depth = max_depth;
    g_search_info.max_time_ms = max_time_ms;
    g_search_info.start_time = get_time_ms();
    g_search_info.stopped = 0;
    g_search_info.use_time = (max_time_ms > 0) ? 1 : 0;

    movesort_clear_killers();

    /* Iterative deepening */
    for (depth = 1; depth <= max_depth; depth++) {
        pv_length[0] = 0;

        score = negamax(-SCORE_INFINITY, SCORE_INFINITY, depth, 0, 1);

        if (g_search_info.stopped) break;

        /* Save results from this completed iteration */
        if (pv_length[0] > 0) {
            best_move_so_far = pv_table[0][0];
        }
        result.best_move = best_move_so_far;
        result.score = score;
        result.depth = depth;
        result.nodes = g_search_info.nodes;

#ifndef TARGET_C64
        /* Print UCI info */
        {
            u32 elapsed = get_time_ms() - g_search_info.start_time;
            u32 nps = elapsed > 0 ? (g_search_info.nodes * 1000 / elapsed) : 0;
            u8 j;
            char from_str[3], to_str[3];

            printf("info depth %d score cp %d nodes %lu time %lu nps %lu pv",
                   depth, score, (unsigned long)g_search_info.nodes,
                   (unsigned long)elapsed, (unsigned long)nps);

            for (j = 0; j < pv_length[0]; j++) {
                sq_to_str(pv_table[0][j].from, from_str);
                sq_to_str(pv_table[0][j].to, to_str);
                printf(" %s%s", from_str, to_str);
                if (pv_table[0][j].flags & MF_PROMO) {
                    static const char promo_chars[] = "nbrq";
                    printf("%c", promo_chars[PROMO_TYPE(pv_table[0][j].flags) - KNIGHT]);
                }
            }
            printf("\n");
            fflush(stdout);
        }
#endif

        /* If we found a forced mate, no need to search deeper */
        if (IS_MATE_SCORE(score)) break;
    }

    return result;
}
