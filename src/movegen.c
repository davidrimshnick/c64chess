#include "movegen.h"
#include "board.h"
#include "tables.h"

/* Add a move to the buffer */
static u16 add_move(u8 ply, u16 count, u8 from, u8 to, u8 flags) {
    u16 idx = g_state.move_buf_idx[ply] + count;
    if (idx >= MOVE_BUF_SIZE) return count; /* overflow protection */
    g_state.move_buf[idx].from = from;
    g_state.move_buf[idx].to = to;
    g_state.move_buf[idx].flags = flags;
    g_state.move_buf[idx].score = 0;
    return count + 1;
}

/* Add pawn promotion moves (4 promotions per target square) */
static u16 add_promotions(u8 ply, u16 count, u8 from, u8 to, u8 capture) {
    u8 base = capture ? MF_CAPTURE : 0;
    count = add_move(ply, count, from, to, (u8)(base | MF_PROMO_Q));
    count = add_move(ply, count, from, to, (u8)(base | MF_PROMO_R));
    count = add_move(ply, count, from, to, (u8)(base | MF_PROMO_B));
    count = add_move(ply, count, from, to, (u8)(base | MF_PROMO_N));
    return count;
}

static u16 generate_pawn_moves(u8 ply, u16 count, u8 side, u8 captures_only) {
    u8 sq, target, piece;
    u8 our_pawn = MAKE_PIECE(side, PAWN);
    s8 forward = (side == WHITE) ? 16 : -16;
    u8 start_rank = (side == WHITE) ? 1 : 6;
    u8 promo_rank = (side == WHITE) ? 7 : 0;
    s8 cap_left  = (side == WHITE) ? 15 : -17;
    s8 cap_right = (side == WHITE) ? 17 : -15;
    u8 opp_color = (side == WHITE) ? COLOR_MASK : 0;

    for (sq = 0; sq < 128; sq++) {
        if (!SQ_VALID(sq)) continue;
        if (g_state.board[sq] != our_pawn) continue;

        /* Captures */
        target = (u8)((s8)sq + cap_left);
        if (SQ_VALID(target)) {
            piece = g_state.board[target];
            if (piece != EMPTY && (piece & COLOR_MASK) == opp_color) {
                if (SQ_RANK(target) == promo_rank) {
                    count = add_promotions(ply, count, sq, target, 1);
                } else {
                    count = add_move(ply, count, sq, target, MF_CAPTURE);
                }
            }
        }

        target = (u8)((s8)sq + cap_right);
        if (SQ_VALID(target)) {
            piece = g_state.board[target];
            if (piece != EMPTY && (piece & COLOR_MASK) == opp_color) {
                if (SQ_RANK(target) == promo_rank) {
                    count = add_promotions(ply, count, sq, target, 1);
                } else {
                    count = add_move(ply, count, sq, target, MF_CAPTURE);
                }
            }
        }

        /* En passant */
        if (g_state.ep_square != SQ_NONE) {
            target = (u8)((s8)sq + cap_left);
            if (target == g_state.ep_square) {
                count = add_move(ply, count, sq, target, (u8)(MF_CAPTURE | MF_EP));
            }
            target = (u8)((s8)sq + cap_right);
            if (target == g_state.ep_square) {
                count = add_move(ply, count, sq, target, (u8)(MF_CAPTURE | MF_EP));
            }
        }

        if (captures_only) continue;

        /* Forward one square */
        target = (u8)((s8)sq + forward);
        if (SQ_VALID(target) && g_state.board[target] == EMPTY) {
            if (SQ_RANK(target) == promo_rank) {
                count = add_promotions(ply, count, sq, target, 0);
            } else {
                count = add_move(ply, count, sq, target, MF_NONE);

                /* Forward two squares from starting rank */
                if (SQ_RANK(sq) == start_rank) {
                    u8 target2 = (u8)((s8)target + forward);
                    if (g_state.board[target2] == EMPTY) {
                        count = add_move(ply, count, sq, target2, MF_PAWNSTART);
                    }
                }
            }
        }
    }
    return count;
}

static u16 generate_knight_moves(u8 ply, u16 count, u8 side, u8 captures_only) {
    u8 sq, target, piece, i;
    u8 our_knight = MAKE_PIECE(side, KNIGHT);
    u8 our_color = side ? COLOR_MASK : 0;

    for (sq = 0; sq < 128; sq++) {
        if (!SQ_VALID(sq)) continue;
        if (g_state.board[sq] != our_knight) continue;

        for (i = 0; i < 8; i++) {
            target = (u8)((s8)sq + knight_offsets[i]);
            if (!SQ_VALID(target)) continue;
            piece = g_state.board[target];
            if (piece != EMPTY && (piece & COLOR_MASK) == our_color) continue;

            if (piece != EMPTY) {
                count = add_move(ply, count, sq, target, MF_CAPTURE);
            } else if (!captures_only) {
                count = add_move(ply, count, sq, target, MF_NONE);
            }
        }
    }
    return count;
}

static u16 generate_sliding_moves(u8 ply, u16 count, u8 side,
                                   const s8 *dirs, u8 num_dirs,
                                   u8 piece_type, u8 captures_only) {
    u8 sq, target, piece, i;
    u8 our_piece = MAKE_PIECE(side, piece_type);
    u8 our_color = side ? COLOR_MASK : 0;
    s8 dir;

    for (sq = 0; sq < 128; sq++) {
        if (!SQ_VALID(sq)) continue;
        if (g_state.board[sq] != our_piece) continue;

        for (i = 0; i < num_dirs; i++) {
            dir = dirs[i];
            target = (u8)((s8)sq + dir);
            while (SQ_VALID(target)) {
                piece = g_state.board[target];
                if (piece != EMPTY) {
                    if ((piece & COLOR_MASK) != our_color) {
                        count = add_move(ply, count, sq, target, MF_CAPTURE);
                    }
                    break;
                }
                if (!captures_only) {
                    count = add_move(ply, count, sq, target, MF_NONE);
                }
                target = (u8)((s8)target + dir);
            }
        }
    }
    return count;
}

static u16 generate_king_moves(u8 ply, u16 count, u8 side, u8 captures_only) {
    static const s8 king_dirs[8] = { -17, -16, -15, -1, 1, 15, 16, 17 };
    u8 sq = g_state.king_sq[side];
    u8 target, piece, i;
    u8 our_color = side ? COLOR_MASK : 0;

    for (i = 0; i < 8; i++) {
        target = (u8)((s8)sq + king_dirs[i]);
        if (!SQ_VALID(target)) continue;
        piece = g_state.board[target];
        if (piece != EMPTY && (piece & COLOR_MASK) == our_color) continue;

        if (piece != EMPTY) {
            count = add_move(ply, count, sq, target, MF_CAPTURE);
        } else if (!captures_only) {
            count = add_move(ply, count, sq, target, MF_NONE);
        }
    }

    /* Castling (not in captures-only mode) */
    if (!captures_only) {
        u8 opp = side ^ 1;
        if (side == WHITE) {
            /* White kingside: e1-g1, f1 and g1 empty, not through check */
            if ((g_state.castle_rights & CASTLE_WK) &&
                g_state.board[SQ_F1] == EMPTY &&
                g_state.board[SQ_G1] == EMPTY &&
                !board_is_square_attacked(SQ_E1, opp) &&
                !board_is_square_attacked(SQ_F1, opp) &&
                !board_is_square_attacked(SQ_G1, opp)) {
                count = add_move(ply, count, SQ_E1, SQ_G1, MF_CASTLE);
            }
            /* White queenside: e1-c1, b1/c1/d1 empty, not through check */
            if ((g_state.castle_rights & CASTLE_WQ) &&
                g_state.board[SQ_D1] == EMPTY &&
                g_state.board[SQ_C1] == EMPTY &&
                g_state.board[SQ_B1] == EMPTY &&
                !board_is_square_attacked(SQ_E1, opp) &&
                !board_is_square_attacked(SQ_D1, opp) &&
                !board_is_square_attacked(SQ_C1, opp)) {
                count = add_move(ply, count, SQ_E1, SQ_C1, MF_CASTLE);
            }
        } else {
            /* Black kingside */
            if ((g_state.castle_rights & CASTLE_BK) &&
                g_state.board[SQ_F8] == EMPTY &&
                g_state.board[SQ_G8] == EMPTY &&
                !board_is_square_attacked(SQ_E8, opp) &&
                !board_is_square_attacked(SQ_F8, opp) &&
                !board_is_square_attacked(SQ_G8, opp)) {
                count = add_move(ply, count, SQ_E8, SQ_G8, MF_CASTLE);
            }
            /* Black queenside */
            if ((g_state.castle_rights & CASTLE_BQ) &&
                g_state.board[SQ_D8] == EMPTY &&
                g_state.board[SQ_C8] == EMPTY &&
                g_state.board[SQ_B8] == EMPTY &&
                !board_is_square_attacked(SQ_E8, opp) &&
                !board_is_square_attacked(SQ_D8, opp) &&
                !board_is_square_attacked(SQ_C8, opp)) {
                count = add_move(ply, count, SQ_E8, SQ_C8, MF_CASTLE);
            }
        }
    }

    return count;
}

static u16 generate_queen_moves(u8 ply, u16 count, u8 side, u8 captures_only) {
    /* Queen combines bishop + rook directions */
    static const s8 queen_dirs[8] = { -17, -16, -15, -1, 1, 15, 16, 17 };
    u8 sq, target, piece, i;
    u8 our_queen = MAKE_PIECE(side, QUEEN);
    u8 our_color = side ? COLOR_MASK : 0;
    s8 dir;

    for (sq = 0; sq < 128; sq++) {
        if (!SQ_VALID(sq)) continue;
        if (g_state.board[sq] != our_queen) continue;

        for (i = 0; i < 8; i++) {
            dir = queen_dirs[i];
            target = (u8)((s8)sq + dir);
            while (SQ_VALID(target)) {
                piece = g_state.board[target];
                if (piece != EMPTY) {
                    if ((piece & COLOR_MASK) != our_color) {
                        count = add_move(ply, count, sq, target, MF_CAPTURE);
                    }
                    break;
                }
                if (!captures_only) {
                    count = add_move(ply, count, sq, target, MF_NONE);
                }
                target = (u8)((s8)target + dir);
            }
        }
    }
    return count;
}

u16 movegen_generate(u8 ply) {
    u16 count = 0;
    u8 side = g_state.side;

    /* Set start index for this ply's moves */
    if (ply == 0) {
        g_state.move_buf_idx[0] = 0;
    }
    g_state.move_buf_idx[ply + 1] = g_state.move_buf_idx[ply]; /* will be updated */

    count = generate_pawn_moves(ply, count, side, 0);
    count = generate_knight_moves(ply, count, side, 0);
    count = generate_sliding_moves(ply, count, side, bishop_offsets, 4, BISHOP, 0);
    count = generate_sliding_moves(ply, count, side, rook_offsets, 4, ROOK, 0);
    count = generate_queen_moves(ply, count, side, 0);
    count = generate_king_moves(ply, count, side, 0);

    /* Update next ply's start index */
    g_state.move_buf_idx[ply + 1] = g_state.move_buf_idx[ply] + count;

    return count;
}

u16 movegen_generate_captures(u8 ply) {
    u16 count = 0;
    u8 side = g_state.side;

    if (ply == 0) {
        g_state.move_buf_idx[0] = 0;
    }
    g_state.move_buf_idx[ply + 1] = g_state.move_buf_idx[ply];

    count = generate_pawn_moves(ply, count, side, 1);
    count = generate_knight_moves(ply, count, side, 1);
    count = generate_sliding_moves(ply, count, side, bishop_offsets, 4, BISHOP, 1);
    count = generate_sliding_moves(ply, count, side, rook_offsets, 4, ROOK, 1);
    count = generate_queen_moves(ply, count, side, 1);
    count = generate_king_moves(ply, count, side, 1);

    g_state.move_buf_idx[ply + 1] = g_state.move_buf_idx[ply] + count;

    return count;
}

u8 movegen_has_legal_move(void) {
    u16 num_moves, i;
    u16 base_idx;

    /* Use a high ply slot to avoid disturbing active search */
    u8 ply = MAX_PLY - 2;
    g_state.move_buf_idx[ply] = MOVE_BUF_SIZE - MAX_MOVES;

    num_moves = movegen_generate(ply);
    base_idx = g_state.move_buf_idx[ply];

    for (i = 0; i < num_moves; i++) {
        if (board_make_move(g_state.move_buf[base_idx + i])) {
            board_unmake_move(g_state.move_buf[base_idx + i]);
            return 1;
        }
    }
    return 0;
}
