#include "board.h"
#include "tables.h"
#include <string.h>

#ifndef TARGET_C64
#include <stdio.h>
#endif

/* Global game state */
GameState g_state;

/* Piece characters for FEN and display */
static const char piece_chars[] = ".PNBRQK..pnbrqk";

static u8 char_to_piece(char c) {
    static const char chars[] = "PNBRQKpnbrqk";
    static const u8 pieces[] = {
        W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
        B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING
    };
    u8 i;
    for (i = 0; i < 12; ++i) {
        if (chars[i] == c) return pieces[i];
    }
    return EMPTY;
}

static s8 get_pst_value(u8 piece, u8 sq88) {
    u8 pt = PIECE_TYPE(piece);
    u8 idx;
    const s8 *tbl;
    if (pt == EMPTY || pt > KING) return 0;
    tbl = pst_table[pt];
    if (!tbl) return 0;
    idx = IS_WHITE(piece) ? SQ_INDEX64(sq88) : SQ_INDEX64(SQ_FLIP(sq88));
    return tbl[idx];
}

void board_init(void) {
    board_set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

u8 board_set_fen(const char *fen) {
    u8 rank, file, sq;
    u8 piece;
    u8 color;
    const char *p = fen;

    memset(&g_state, 0, sizeof(GameState));
    g_state.ep_square = SQ_NONE;

    /* Parse piece placement */
    rank = 7;
    file = 0;
    while (*p && *p != ' ') {
        if (*p == '/') {
            if (rank == 0) break;
            rank--;
            file = 0;
        } else if (*p >= '1' && *p <= '8') {
            file += (u8)(*p - '0');
        } else {
            piece = char_to_piece(*p);
            if (piece != EMPTY) {
                sq = SQ_MAKE(rank, file);
                g_state.board[sq] = piece;
                color = PIECE_COLOR(piece);

                /* Update material and PST scores */
                g_state.material[color] += material_value[PIECE_TYPE(piece)];
                g_state.pst_score[color] += get_pst_value(piece, sq);

                /* Track king position */
                if (PIECE_TYPE(piece) == KING) {
                    g_state.king_sq[color] = sq;
                }
            }
            file++;
        }
        p++;
    }

    /* Side to move */
    if (*p) p++; /* skip space */
    if (*p == 'b') {
        g_state.side = BLACK;
    } else {
        g_state.side = WHITE;
    }
    if (*p) p++;

    /* Castling rights */
    if (*p) p++; /* skip space */
    g_state.castle_rights = 0;
    while (*p && *p != ' ') {
        switch (*p) {
            case 'K': g_state.castle_rights |= CASTLE_WK; break;
            case 'Q': g_state.castle_rights |= CASTLE_WQ; break;
            case 'k': g_state.castle_rights |= CASTLE_BK; break;
            case 'q': g_state.castle_rights |= CASTLE_BQ; break;
            default: break;
        }
        p++;
    }

    /* En passant square */
    if (*p) p++; /* skip space */
    if (*p == '-') {
        g_state.ep_square = SQ_NONE;
        p++;
    } else if (*p >= 'a' && *p <= 'h') {
        file = (u8)(*p - 'a');
        p++;
        if (*p >= '1' && *p <= '8') {
            rank = (u8)(*p - '1');
            g_state.ep_square = SQ_MAKE(rank, file);
            p++;
        }
    }

    /* Fifty-move clock */
    if (*p) p++; /* skip space */
    g_state.fifty_clock = 0;
    while (*p && *p != ' ') {
        g_state.fifty_clock = g_state.fifty_clock * 10 + (u8)(*p - '0');
        p++;
    }

    /* Full move number (skip, we derive ply) */
    if (*p) p++;
    {
        u16 fullmove = 0;
        while (*p && *p != ' ') {
            fullmove = fullmove * 10 + (u16)(*p - '0');
            p++;
        }
        g_state.ply = (fullmove - 1) * 2 + g_state.side;
    }

    /* Compute Zobrist hash */
    g_state.hash = board_compute_hash();
    g_state.undo_ply = 0;
    g_state.hash_hist_count = 0;

    return 1;
}

void board_get_fen(char *buf) {
    s8 rank;
    u8 file, empty, sq, piece;
    char *p = buf;

    for (rank = 7; rank >= 0; rank--) {
        empty = 0;
        for (file = 0; file < 8; file++) {
            sq = SQ_MAKE(rank, file);
            piece = g_state.board[sq];
            if (piece == EMPTY) {
                empty++;
            } else {
                if (empty > 0) {
                    *p++ = '0' + empty;
                    empty = 0;
                }
                if (IS_WHITE(piece)) {
                    *p++ = piece_chars[PIECE_TYPE(piece)];
                } else {
                    *p++ = piece_chars[PIECE_TYPE(piece) + 8];
                }
            }
        }
        if (empty > 0) *p++ = '0' + empty;
        if (rank > 0) *p++ = '/';
    }

    *p++ = ' ';
    *p++ = g_state.side == WHITE ? 'w' : 'b';
    *p++ = ' ';

    if (g_state.castle_rights == 0) {
        *p++ = '-';
    } else {
        if (g_state.castle_rights & CASTLE_WK) *p++ = 'K';
        if (g_state.castle_rights & CASTLE_WQ) *p++ = 'Q';
        if (g_state.castle_rights & CASTLE_BK) *p++ = 'k';
        if (g_state.castle_rights & CASTLE_BQ) *p++ = 'q';
    }

    *p++ = ' ';
    if (g_state.ep_square == SQ_NONE) {
        *p++ = '-';
    } else {
        *p++ = file_to_char(SQ_FILE(g_state.ep_square));
        *p++ = rank_to_char(SQ_RANK(g_state.ep_square));
    }

    /* fifty clock and full move number */
    {
        u8 fc = g_state.fifty_clock;
        u16 fm = g_state.ply / 2 + 1;
        char tmp[8];
        s8 i, len;

        *p++ = ' ';
        /* write fifty clock */
        if (fc == 0) {
            *p++ = '0';
        } else {
            len = 0;
            while (fc > 0) { tmp[len++] = '0' + (fc % 10); fc /= 10; }
            for (i = len - 1; i >= 0; i--) *p++ = tmp[i];
        }

        *p++ = ' ';
        /* write full move number */
        if (fm == 0) {
            *p++ = '1';
        } else {
            len = 0;
            while (fm > 0) { tmp[len++] = '0' + (char)(fm % 10); fm /= 10; }
            for (i = len - 1; i >= 0; i--) *p++ = tmp[i];
        }
    }

    *p = '\0';
}

u8 board_make_move(Move m) {
    u8 from = m.from;
    u8 to = m.to;
    u8 flags = m.flags;
    u8 piece = g_state.board[from];
    u8 captured = g_state.board[to];
    u8 side = g_state.side;
    u8 opp = side ^ 1;
    u8 pt = PIECE_TYPE(piece);
    Undo *undo;

    /* Save undo info */
    undo = &g_state.undo_stack[g_state.undo_ply];
    undo->captured = captured;
    undo->castle_rights = g_state.castle_rights;
    undo->ep_square = g_state.ep_square;
    undo->fifty_clock = g_state.fifty_clock;
    undo->hash = g_state.hash;
    undo->material[0] = g_state.material[0];
    undo->material[1] = g_state.material[1];
    undo->pst_score[0] = g_state.pst_score[0];
    undo->pst_score[1] = g_state.pst_score[1];

    /* Save hash for repetition detection */
    g_state.hash_history[g_state.hash_hist_count++] = g_state.hash;

    /* Update fifty-move clock */
    g_state.fifty_clock++;
    if (pt == PAWN || captured != EMPTY) {
        g_state.fifty_clock = 0;
    }

    /* Remove piece from source */
    g_state.hash ^= zobrist_pieces[side][pt][from];
    g_state.pst_score[side] -= get_pst_value(piece, from);
    g_state.board[from] = EMPTY;

    /* Handle capture */
    if (captured != EMPTY) {
        u8 cap_type = PIECE_TYPE(captured);
        g_state.hash ^= zobrist_pieces[opp][cap_type][to];
        g_state.material[opp] -= material_value[cap_type];
        g_state.pst_score[opp] -= get_pst_value(captured, to);
    }

    /* Handle en passant capture */
    if (flags & MF_EP) {
        u8 ep_cap_sq = (side == WHITE) ? (u8)(to - 16) : (u8)(to + 16);
        u8 ep_piece = g_state.board[ep_cap_sq];
        if (ep_piece != EMPTY) {
            u8 ep_type = PIECE_TYPE(ep_piece);
            g_state.hash ^= zobrist_pieces[opp][ep_type][ep_cap_sq];
            g_state.material[opp] -= material_value[ep_type];
            g_state.pst_score[opp] -= get_pst_value(ep_piece, ep_cap_sq);
            g_state.board[ep_cap_sq] = EMPTY;
            undo->captured = ep_piece;
        }
    }

    /* Handle promotion */
    if (flags & MF_PROMO) {
        u8 promo_type = PROMO_TYPE(flags);
        u8 promo_piece = MAKE_PIECE(side, promo_type);
        g_state.board[to] = promo_piece;
        g_state.hash ^= zobrist_pieces[side][promo_type][to];
        g_state.pst_score[side] += get_pst_value(promo_piece, to);
        /* Adjust material: remove pawn value, add promotion piece value */
        g_state.material[side] -= material_value[PAWN];
        g_state.material[side] += material_value[promo_type];
    } else {
        /* Normal move: place piece at destination */
        g_state.board[to] = piece;
        g_state.hash ^= zobrist_pieces[side][pt][to];
        g_state.pst_score[side] += get_pst_value(piece, to);
    }

    /* Update king position */
    if (pt == KING) {
        g_state.king_sq[side] = to;
    }

    /* Handle castling move (move the rook) */
    if (flags & MF_CASTLE) {
        u8 rook_from, rook_to;
        u8 rook_piece = MAKE_PIECE(side, ROOK);
        if (to > from) {
            /* Kingside */
            rook_from = (u8)(from + 3);
            rook_to = (u8)(from + 1);
        } else {
            /* Queenside */
            rook_from = (u8)(from - 4);
            rook_to = (u8)(from - 1);
        }
        g_state.board[rook_from] = EMPTY;
        g_state.board[rook_to] = rook_piece;
        g_state.hash ^= zobrist_pieces[side][ROOK][rook_from];
        g_state.hash ^= zobrist_pieces[side][ROOK][rook_to];
        g_state.pst_score[side] -= get_pst_value(rook_piece, rook_from);
        g_state.pst_score[side] += get_pst_value(rook_piece, rook_to);
    }

    /* Update castling rights */
    g_state.hash ^= zobrist_castle[g_state.castle_rights];
    g_state.castle_rights &= castle_mask[from] & castle_mask[to];
    g_state.hash ^= zobrist_castle[g_state.castle_rights];

    /* Update en passant square */
    if (g_state.ep_square != SQ_NONE) {
        g_state.hash ^= zobrist_ep[SQ_FILE(g_state.ep_square)];
    }
    if ((flags & MF_PAWNSTART) && pt == PAWN) {
        g_state.ep_square = (side == WHITE) ? (u8)(from + 16) : (u8)(from - 16);
        g_state.hash ^= zobrist_ep[SQ_FILE(g_state.ep_square)];
    } else {
        g_state.ep_square = SQ_NONE;
    }

    /* Switch side */
    g_state.side ^= 1;
    g_state.hash ^= zobrist_side;
    g_state.ply++;
    g_state.undo_ply++;

    /* Verify legality: king must not be in check */
    if (board_is_square_attacked(g_state.king_sq[side], opp)) {
        board_unmake_move(m);
        return 0; /* illegal move */
    }

    return 1; /* legal */
}

void board_unmake_move(Move m) {
    u8 from = m.from;
    u8 to = m.to;
    u8 flags = m.flags;
    u8 side, pt;
    Undo *undo;

    /* Switch back */
    g_state.side ^= 1;
    g_state.ply--;
    g_state.undo_ply--;
    g_state.hash_hist_count--;

    side = g_state.side;
    undo = &g_state.undo_stack[g_state.undo_ply];

    /* Restore state */
    g_state.castle_rights = undo->castle_rights;
    g_state.ep_square = undo->ep_square;
    g_state.fifty_clock = undo->fifty_clock;
    g_state.hash = undo->hash;
    g_state.material[0] = undo->material[0];
    g_state.material[1] = undo->material[1];
    g_state.pst_score[0] = undo->pst_score[0];
    g_state.pst_score[1] = undo->pst_score[1];

    /* Handle promotion: piece on 'to' is promoted piece, restore pawn */
    if (flags & MF_PROMO) {
        g_state.board[from] = MAKE_PIECE(side, PAWN);
    } else {
        g_state.board[from] = g_state.board[to];
    }

    pt = PIECE_TYPE(g_state.board[from]);

    /* Update king position */
    if (pt == KING) {
        g_state.king_sq[side] = from;
    }

    /* Handle en passant: restore captured pawn to correct square */
    if (flags & MF_EP) {
        u8 ep_cap_sq = (side == WHITE) ? (u8)(to - 16) : (u8)(to + 16);
        g_state.board[to] = EMPTY;
        g_state.board[ep_cap_sq] = undo->captured;
    } else {
        /* Restore captured piece (or EMPTY) to target square */
        g_state.board[to] = undo->captured;
    }

    /* Handle castling: move rook back */
    if (flags & MF_CASTLE) {
        u8 rook_from, rook_to;
        u8 rook_piece = MAKE_PIECE(side, ROOK);
        if (to > from) {
            rook_from = (u8)(from + 3);
            rook_to = (u8)(from + 1);
        } else {
            rook_from = (u8)(from - 4);
            rook_to = (u8)(from - 1);
        }
        g_state.board[rook_to] = EMPTY;
        g_state.board[rook_from] = rook_piece;
    }
}

void board_make_null(void) {
    Undo *undo = &g_state.undo_stack[g_state.undo_ply];

    undo->captured = EMPTY;
    undo->castle_rights = g_state.castle_rights;
    undo->ep_square = g_state.ep_square;
    undo->fifty_clock = g_state.fifty_clock;
    undo->hash = g_state.hash;
    undo->material[0] = g_state.material[0];
    undo->material[1] = g_state.material[1];
    undo->pst_score[0] = g_state.pst_score[0];
    undo->pst_score[1] = g_state.pst_score[1];

    g_state.hash_history[g_state.hash_hist_count++] = g_state.hash;

    if (g_state.ep_square != SQ_NONE) {
        g_state.hash ^= zobrist_ep[SQ_FILE(g_state.ep_square)];
        g_state.ep_square = SQ_NONE;
    }

    g_state.side ^= 1;
    g_state.hash ^= zobrist_side;
    g_state.ply++;
    g_state.undo_ply++;
}

void board_unmake_null(void) {
    g_state.side ^= 1;
    g_state.ply--;
    g_state.undo_ply--;
    g_state.hash_hist_count--;

    {
        Undo *undo = &g_state.undo_stack[g_state.undo_ply];
        g_state.castle_rights = undo->castle_rights;
        g_state.ep_square = undo->ep_square;
        g_state.fifty_clock = undo->fifty_clock;
        g_state.hash = undo->hash;
        g_state.material[0] = undo->material[0];
        g_state.material[1] = undo->material[1];
        g_state.pst_score[0] = undo->pst_score[0];
        g_state.pst_score[1] = undo->pst_score[1];
    }
}

u8 board_is_square_attacked(u8 sq, u8 by_side) {
    u8 i, target_sq, piece, pt;
    s8 dir;
    u8 opp_color = by_side ? COLOR_MASK : 0;

    /* Check knight attacks */
    for (i = 0; i < 8; i++) {
        target_sq = (u8)((s8)sq + knight_offsets[i]);
        if (SQ_VALID(target_sq)) {
            piece = g_state.board[target_sq];
            if (piece != EMPTY && PIECE_TYPE(piece) == KNIGHT &&
                (piece & COLOR_MASK) == opp_color) {
                return 1;
            }
        }
    }

    /* Check pawn attacks */
    if (by_side == WHITE) {
        /* White pawns attack diagonally upward from their position */
        target_sq = (u8)((s8)sq - 15); /* down-right from target = pawn is SW */
        if (SQ_VALID(target_sq) && g_state.board[target_sq] == W_PAWN) return 1;
        target_sq = (u8)((s8)sq - 17); /* down-left from target = pawn is SE */
        if (SQ_VALID(target_sq) && g_state.board[target_sq] == W_PAWN) return 1;
    } else {
        /* Black pawns attack diagonally downward */
        target_sq = (u8)((s8)sq + 15); /* up-left from target = pawn is NE */
        if (SQ_VALID(target_sq) && g_state.board[target_sq] == B_PAWN) return 1;
        target_sq = (u8)((s8)sq + 17); /* up-right from target = pawn is NW */
        if (SQ_VALID(target_sq) && g_state.board[target_sq] == B_PAWN) return 1;
    }

    /* Check king attacks (adjacent squares) */
    {
        static const s8 king_dirs[8] = { -17, -16, -15, -1, 1, 15, 16, 17 };
        for (i = 0; i < 8; i++) {
            target_sq = (u8)((s8)sq + king_dirs[i]);
            if (SQ_VALID(target_sq)) {
                piece = g_state.board[target_sq];
                if (piece != EMPTY && PIECE_TYPE(piece) == KING &&
                    (piece & COLOR_MASK) == opp_color) {
                    return 1;
                }
            }
        }
    }

    /* Check sliding piece attacks: bishop/queen on diagonals */
    for (i = 0; i < 4; i++) {
        dir = bishop_offsets[i];
        target_sq = (u8)((s8)sq + dir);
        while (SQ_VALID(target_sq)) {
            piece = g_state.board[target_sq];
            if (piece != EMPTY) {
                if ((piece & COLOR_MASK) == opp_color) {
                    pt = PIECE_TYPE(piece);
                    if (pt == BISHOP || pt == QUEEN) return 1;
                }
                break;
            }
            target_sq = (u8)((s8)target_sq + dir);
        }
    }

    /* Check sliding piece attacks: rook/queen on ranks/files */
    for (i = 0; i < 4; i++) {
        dir = rook_offsets[i];
        target_sq = (u8)((s8)sq + dir);
        while (SQ_VALID(target_sq)) {
            piece = g_state.board[target_sq];
            if (piece != EMPTY) {
                if ((piece & COLOR_MASK) == opp_color) {
                    pt = PIECE_TYPE(piece);
                    if (pt == ROOK || pt == QUEEN) return 1;
                }
                break;
            }
            target_sq = (u8)((s8)target_sq + dir);
        }
    }

    return 0;
}

u8 board_in_check(void) {
    return board_is_square_attacked(g_state.king_sq[g_state.side],
                                     g_state.side ^ 1);
}

u8 board_is_repetition(void) {
    u16 i;
    u8 count = 0;
    u16 current_hash = g_state.hash;

    /* Only check back to last irreversible move (fifty_clock positions) */
    if (g_state.hash_hist_count < 4) return 0;

    for (i = g_state.hash_hist_count; i > 0; i--) {
        if (g_state.hash_history[i - 1] == current_hash) {
            count++;
            if (count >= 2) return 1; /* third occurrence including current */
        }
    }
    return 0;
}

u16 board_compute_hash(void) {
    u16 hash = 0;
    u8 sq, piece, color, pt;

    for (sq = 0; sq < 128; sq++) {
        if (!SQ_VALID(sq)) continue;
        piece = g_state.board[sq];
        if (piece == EMPTY) continue;
        color = PIECE_COLOR(piece);
        pt = PIECE_TYPE(piece);
        hash ^= zobrist_pieces[color][pt][sq];
    }

    if (g_state.side == BLACK) {
        hash ^= zobrist_side;
    }
    hash ^= zobrist_castle[g_state.castle_rights];
    if (g_state.ep_square != SQ_NONE) {
        hash ^= zobrist_ep[SQ_FILE(g_state.ep_square)];
    }

    return hash;
}

char file_to_char(u8 file) { return (char)('a' + file); }
char rank_to_char(u8 rank) { return (char)('1' + rank); }

void sq_to_str(u8 sq, char *buf) {
    buf[0] = file_to_char(SQ_FILE(sq));
    buf[1] = rank_to_char(SQ_RANK(sq));
    buf[2] = '\0';
}

#ifndef TARGET_C64
void board_print(void) {
    s8 rank;
    u8 file, sq, piece;

    printf("\n  +---+---+---+---+---+---+---+---+\n");
    for (rank = 7; rank >= 0; rank--) {
        printf("%d |", rank + 1);
        for (file = 0; file < 8; file++) {
            sq = SQ_MAKE(rank, file);
            piece = g_state.board[sq];
            if (piece == EMPTY) {
                printf("   |");
            } else {
                u8 idx = PIECE_TYPE(piece);
                if (IS_BLACK(piece)) idx += 8;
                printf(" %c |", piece_chars[idx]);
            }
        }
        printf("\n  +---+---+---+---+---+---+---+---+\n");
    }
    printf("    a   b   c   d   e   f   g   h\n\n");
    printf("Side: %s  Castle: %c%c%c%c  EP: ",
        g_state.side == WHITE ? "White" : "Black",
        (g_state.castle_rights & CASTLE_WK) ? 'K' : '-',
        (g_state.castle_rights & CASTLE_WQ) ? 'Q' : '-',
        (g_state.castle_rights & CASTLE_BK) ? 'k' : '-',
        (g_state.castle_rights & CASTLE_BQ) ? 'q' : '-');
    if (g_state.ep_square == SQ_NONE) {
        printf("-");
    } else {
        printf("%c%c", file_to_char(SQ_FILE(g_state.ep_square)),
                       rank_to_char(SQ_RANK(g_state.ep_square)));
    }
    printf("  Hash: %04X\n\n", g_state.hash);
}
#endif
