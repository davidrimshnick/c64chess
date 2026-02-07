#ifdef TARGET_C64

#include "ui.h"
#include "platform.h"
#include "../board.h"
#include "../movegen.h"
#include "../search.h"
#include "../eval.h"
#include "../tt.h"
#include "../tables.h"
#include <string.h>

/*
 * Board layout on 40x25 PETSCII screen:
 * Rows 0-1:   Title
 * Rows 2-17:  Chess board (8 ranks x 2 rows each)
 * Row 18:     File labels
 * Rows 19-20: Status/info
 * Rows 21-22: Move input
 * Row 24:     Prompt
 */

#define BOARD_X 4    /* left edge of board on screen */
#define BOARD_Y 2    /* top edge of board on screen */

/* PETSCII screen codes for chess pieces (uppercase mode) */
/* Using letters: K Q R B N P for white, reversed for black */
static const u8 piece_screen_codes[7] = {
    0x2E,  /* '.' empty (dot) */
    0x10,  /* 'P' */
    0x0E,  /* 'N' */
    0x02,  /* 'B' */
    0x12,  /* 'R' */
    0x11,  /* 'Q' */
    0x0B   /* 'K' */
};

void ui_draw_board(void) {
    s8 rank;
    u8 file, sq, piece, pt;
    u8 scr_row, scr_col;
    u8 ch, color;
    u8 dark_sq;

    /* Draw rank labels and board squares */
    for (rank = 7; rank >= 0; rank--) {
        scr_row = BOARD_Y + (u8)(7 - rank) * 2;

        /* Rank number */
        platform_putc(BOARD_X - 2, scr_row, (u8)('1' + rank - 0x40 + 0x30), COLOR_LGRAY);

        for (file = 0; file < 8; file++) {
            sq = SQ_MAKE(rank, file);
            piece = g_state.board[sq];
            pt = PIECE_TYPE(piece);
            scr_col = BOARD_X + file * 4;

            /* Determine square color */
            dark_sq = ((rank + file) & 1) == 0;

            if (piece == EMPTY) {
                ch = dark_sq ? 0xA0 : 0x20; /* reversed space or space */
                color = COLOR_DGRAY;
            } else {
                ch = piece_screen_codes[pt];
                if (IS_BLACK(piece)) {
                    color = dark_sq ? COLOR_RED : COLOR_RED;
                } else {
                    color = dark_sq ? COLOR_WHITE : COLOR_CYAN;
                }
            }

            /* Draw piece centered in 3-char wide cell */
            {
                u8 bg_ch = dark_sq ? 0xA0 : 0x20;
                u8 bg_color = dark_sq ? COLOR_DGRAY : COLOR_BLACK;
                platform_putc(scr_col, scr_row, bg_ch, bg_color);
                platform_putc(scr_col + 1, scr_row, ch, color);
                platform_putc(scr_col + 2, scr_row, bg_ch, bg_color);
                platform_putc(scr_col + 3, scr_row, 0x20, COLOR_BLACK);

                /* Extra row for height */
                platform_putc(scr_col, scr_row + 1, bg_ch, bg_color);
                platform_putc(scr_col + 1, scr_row + 1, bg_ch, bg_color);
                platform_putc(scr_col + 2, scr_row + 1, bg_ch, bg_color);
                platform_putc(scr_col + 3, scr_row + 1, 0x20, COLOR_BLACK);
            }
        }
    }

    /* File labels */
    {
        u8 label_row = BOARD_Y + 16;
        for (file = 0; file < 8; file++) {
            platform_putc(BOARD_X + file * 4 + 1, label_row,
                         (u8)(0x01 + file), COLOR_LGRAY); /* a-h in PETSCII */
        }
    }

    /* Side to move indicator */
    platform_puts(BOARD_X + 34, BOARD_Y,
                  g_state.side == WHITE ? "WHITE" : "BLACK", COLOR_YELLOW);
}

void ui_status(const char *msg) {
    u8 i;
    /* Clear status line */
    for (i = 0; i < 40; i++) {
        platform_putc(i, 20, 0x20, COLOR_BLACK);
    }
    platform_puts(0, 20, msg, COLOR_YELLOW);
}

void ui_show_thinking(u8 depth, s16 score) {
    /* Show depth and score on info line */
    u8 col = 0;
    platform_puts(col, 19, "D:", COLOR_LGRAY);
    col += 2;
    if (depth >= 10) {
        platform_putc(col++, 19, (u8)(0x30 + depth / 10), COLOR_LGRAY);
    }
    platform_putc(col++, 19, (u8)(0x30 + depth % 10), COLOR_LGRAY);

    platform_puts(col, 19, " S:", COLOR_LGRAY);
    col += 3;

    /* Simple score display */
    if (score < 0) {
        platform_putc(col++, 19, 0x2D, COLOR_LGRAY); /* '-' */
        score = -score;
    }
    if (score >= 1000) {
        platform_putc(col++, 19, (u8)(0x30 + (score / 1000) % 10), COLOR_LGRAY);
    }
    if (score >= 100) {
        platform_putc(col++, 19, (u8)(0x30 + (score / 100) % 10), COLOR_LGRAY);
    }
    if (score >= 10) {
        platform_putc(col++, 19, (u8)(0x30 + (score / 10) % 10), COLOR_LGRAY);
    }
    platform_putc(col++, 19, (u8)(0x30 + score % 10), COLOR_LGRAY);
}

static u8 parse_square(u8 file_ch, u8 rank_ch) {
    u8 file, rank;
    if (file_ch >= 'a' && file_ch <= 'h') {
        file = file_ch - 'a';
    } else if (file_ch >= 'A' && file_ch <= 'H') {
        file = file_ch - 'A';
    } else {
        return SQ_NONE;
    }
    if (rank_ch >= '1' && rank_ch <= '8') {
        rank = rank_ch - '1';
    } else {
        return SQ_NONE;
    }
    return SQ_MAKE(rank, file);
}

u8 ui_get_move(Move *m) {
    u8 input[5];
    u8 i, ch;
    u8 from, to;
    u16 num_moves, base_idx;

    ui_status("YOUR MOVE (E.G. E2E4):");

    /* Read 4 characters */
    for (i = 0; i < 4; i++) {
        ch = platform_getkey();
        if (ch == 0x0D && i == 0) return 0; /* Enter with no input = quit */
        input[i] = ch;
        platform_putc(23 + i, 20, ch, COLOR_GREEN);
    }
    input[4] = 0;

    from = parse_square(input[0], input[1]);
    to = parse_square(input[2], input[3]);

    if (from == SQ_NONE || to == SQ_NONE) {
        ui_status("INVALID SQUARE!");
        return 0;
    }

    /* Find matching legal move */
    g_state.move_buf_idx[0] = 0;
    num_moves = movegen_generate(0);
    base_idx = g_state.move_buf_idx[0];

    for (i = 0; i < num_moves; i++) {
        if (g_state.move_buf[base_idx + i].from == from &&
            g_state.move_buf[base_idx + i].to == to) {
            /* For promotions, default to queen */
            if (g_state.move_buf[base_idx + i].flags & MF_PROMO) {
                if ((g_state.move_buf[base_idx + i].flags & 0x60) == (3 << 5)) {
                    /* Queen promotion */
                    *m = g_state.move_buf[base_idx + i];
                    if (board_make_move(*m)) {
                        return 1;
                    }
                    board_unmake_move(*m);
                }
                continue;
            }
            *m = g_state.move_buf[base_idx + i];
            if (board_make_move(*m)) {
                return 1;
            }
            board_unmake_move(*m);
        }
    }

    ui_status("ILLEGAL MOVE!");
    return 0;
}

void ui_game_loop(void) {
    u8 game_over = 0;
    u8 human_side = WHITE;
    Move m;
    SearchResult result;

    platform_init();
    board_init();
    tt_clear();

    platform_puts(10, 0, "C64 CHESS ENGINE", COLOR_CYAN);

    while (!game_over) {
        ui_draw_board();

        /* Check for game over */
        if (!movegen_has_legal_move()) {
            if (board_in_check()) {
                ui_status(g_state.side == WHITE ?
                    "CHECKMATE! BLACK WINS!" : "CHECKMATE! WHITE WINS!");
            } else {
                ui_status("STALEMATE! DRAW!");
            }
            platform_getkey();
            break;
        }

        if (g_state.fifty_clock >= 100) {
            ui_status("DRAW BY 50-MOVE RULE!");
            platform_getkey();
            break;
        }

        if (board_is_repetition()) {
            ui_status("DRAW BY REPETITION!");
            platform_getkey();
            break;
        }

        if (board_in_check()) {
            ui_status("CHECK!");
        }

        if (g_state.side == human_side) {
            /* Human's turn */
            while (!ui_get_move(&m)) {
                /* Keep trying until valid move or quit */
            }
        } else {
            /* Computer's turn */
            ui_status("THINKING...");

            /* Search with time limit: 15 seconds on C64 */
            result = search_position(20, 15000);

            if (result.best_move.from == 0 && result.best_move.to == 0) {
                ui_status("NO MOVE FOUND!");
                platform_getkey();
                break;
            }

            ui_show_thinking(result.depth, result.score);

            if (!board_make_move(result.best_move)) {
                ui_status("ENGINE ERROR!");
                platform_getkey();
                break;
            }
        }
    }
}

#endif /* TARGET_C64 */
