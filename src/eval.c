#include "eval.h"
#include "tables.h"

u8 eval_is_endgame(void) {
    /* Endgame if no queens, or each side has queen + at most 1 minor */
    s16 white_non_pawn = g_state.material[WHITE] - 20000; /* remove king */
    s16 black_non_pawn = g_state.material[BLACK] - 20000;

    /* Remove pawn material (approximate: count board) */
    /* Simple heuristic: endgame if non-pawn-king material <= queen + minor */
    if (white_non_pawn <= 1300 && black_non_pawn <= 1300) return 1;
    return 0;
}

s16 eval_position(void) {
    s16 score;
    s16 white_score, black_score;

    /* Material + PST (incrementally updated) */
    white_score = g_state.material[WHITE] + g_state.pst_score[WHITE];
    black_score = g_state.material[BLACK] + g_state.pst_score[BLACK];

    /* Endgame king PST adjustment */
    if (eval_is_endgame()) {
        u8 wk_sq64 = SQ_INDEX64(g_state.king_sq[WHITE]);
        u8 bk_sq64 = SQ_INDEX64(SQ_FLIP(g_state.king_sq[BLACK]));

        /* Remove middlegame king PST, add endgame king PST */
        white_score -= pst_king_mg[wk_sq64];
        white_score += pst_king_eg[wk_sq64];
        black_score -= pst_king_mg[bk_sq64];
        black_score += pst_king_eg[bk_sq64];
    }

    /* Score relative to side to move */
    score = white_score - black_score;
    if (g_state.side == BLACK) score = -score;

    return score;
}
