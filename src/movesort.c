#include "movesort.h"
#include "tables.h"

/* Killer moves: 2 per ply */
static Move killers[MAX_PLY][2];

void movesort_clear_killers(void) {
    u8 i;
    for (i = 0; i < MAX_PLY; i++) {
        killers[i][0].from = 0;
        killers[i][0].to = 0;
        killers[i][0].flags = 0;
        killers[i][0].score = 0;
        killers[i][1].from = 0;
        killers[i][1].to = 0;
        killers[i][1].flags = 0;
        killers[i][1].score = 0;
    }
}

static u8 moves_equal(Move a, Move b) {
    return (a.from == b.from && a.to == b.to && a.flags == b.flags);
}

void movesort_score_moves(u8 ply, u16 num_moves, Move *pv_move) {
    u16 base = g_state.move_buf_idx[ply];
    u16 i;

    for (i = 0; i < num_moves; i++) {
        Move *m = &g_state.move_buf[base + i];

        /* PV move gets highest score */
        if (pv_move && m->from == pv_move->from &&
            m->to == pv_move->to && m->flags == pv_move->flags) {
            m->score = 255;
            continue;
        }

        /* Captures: scored by MVV-LVA */
        if (m->flags & MF_CAPTURE) {
            u8 victim, attacker;
            if (m->flags & MF_EP) {
                victim = PAWN;
            } else {
                victim = PIECE_TYPE(g_state.board[m->to]);
            }
            attacker = PIECE_TYPE(g_state.board[m->from]);
            m->score = 200 + mvv_lva[victim][attacker];
            continue;
        }

        /* Promotions (non-capture) */
        if (m->flags & MF_PROMO) {
            m->score = 190 + PROMO_TYPE(m->flags);
            continue;
        }

        /* Killer moves */
        if (ply < MAX_PLY) {
            if (moves_equal(*m, killers[ply][0])) {
                m->score = 150;
                continue;
            }
            if (moves_equal(*m, killers[ply][1])) {
                m->score = 140;
                continue;
            }
        }

        /* Quiet moves */
        m->score = 0;
    }
}

void movesort_pick_best(u8 ply, u16 idx, u16 num_moves) {
    u16 base = g_state.move_buf_idx[ply];
    u16 best_i = idx;
    u8 best_score = g_state.move_buf[base + idx].score;
    u16 i;
    Move tmp;

    for (i = idx + 1; i < num_moves; i++) {
        if (g_state.move_buf[base + i].score > best_score) {
            best_score = g_state.move_buf[base + i].score;
            best_i = i;
        }
    }

    if (best_i != idx) {
        tmp = g_state.move_buf[base + idx];
        g_state.move_buf[base + idx] = g_state.move_buf[base + best_i];
        g_state.move_buf[base + best_i] = tmp;
    }
}

void movesort_update_killers(u8 ply, Move m) {
    if (ply >= MAX_PLY) return;
    /* Don't store captures as killers */
    if (m.flags & MF_CAPTURE) return;

    /* Shift killer[0] to killer[1], store new in killer[0] */
    if (!moves_equal(m, killers[ply][0])) {
        killers[ply][1] = killers[ply][0];
        killers[ply][0] = m;
    }
}
