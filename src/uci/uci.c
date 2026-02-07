#ifndef TARGET_C64

#include "uci.h"
#include "../board.h"
#include "../movegen.h"
#include "../search.h"
#include "../eval.h"
#include "../tt.h"
#include "../tables.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *ENGINE_NAME = "C64Chess 1.0";
static const char *ENGINE_AUTHOR = "David";

u8 uci_parse_move(const char *str, Move *m) {
    u8 from, to;
    u16 num_moves, base_idx, i;

    if (strlen(str) < 4) return 0;

    from = SQ_MAKE(str[1] - '1', str[0] - 'a');
    to = SQ_MAKE(str[3] - '1', str[2] - 'a');

    if (!SQ_VALID(from) || !SQ_VALID(to)) return 0;

    /* Generate all moves and find the matching one */
    g_state.move_buf_idx[0] = 0;
    num_moves = movegen_generate(0);
    base_idx = g_state.move_buf_idx[0];

    for (i = 0; i < num_moves; i++) {
        Move *mv = &g_state.move_buf[base_idx + i];
        if (mv->from == from && mv->to == to) {
            /* Check promotion piece */
            if (mv->flags & MF_PROMO) {
                char promo_ch = (strlen(str) >= 5) ? str[4] : 'q';
                u8 promo_type;
                switch (promo_ch) {
                    case 'n': promo_type = KNIGHT; break;
                    case 'b': promo_type = BISHOP; break;
                    case 'r': promo_type = ROOK; break;
                    default:  promo_type = QUEEN; break;
                }
                if (PROMO_TYPE(mv->flags) == promo_type) {
                    *m = *mv;
                    return 1;
                }
                continue;
            }
            *m = *mv;
            return 1;
        }
    }
    return 0;
}

void uci_format_move(Move m, char *buf) {
    buf[0] = (char)('a' + SQ_FILE(m.from));
    buf[1] = (char)('1' + SQ_RANK(m.from));
    buf[2] = (char)('a' + SQ_FILE(m.to));
    buf[3] = (char)('1' + SQ_RANK(m.to));
    if (m.flags & MF_PROMO) {
        static const char promo_chars[] = "nbrq";
        buf[4] = promo_chars[PROMO_TYPE(m.flags) - KNIGHT];
        buf[5] = '\0';
    } else {
        buf[4] = '\0';
    }
}

static void uci_cmd_position(const char *line) {
    const char *p = line;
    Move m;

    /* Skip "position " */
    while (*p == ' ') p++;

    if (strncmp(p, "startpos", 8) == 0) {
        board_init();
        p += 8;
    } else if (strncmp(p, "fen", 3) == 0) {
        p += 3;
        while (*p == ' ') p++;
        board_set_fen(p);
        /* Skip past FEN (6 space-separated fields) */
        {
            u8 spaces = 0;
            while (*p && spaces < 6) {
                if (*p == ' ') spaces++;
                p++;
            }
            if (spaces < 6) {
                while (*p && *p != ' ') p++;
            }
        }
    }

    /* Parse moves if present */
    while (*p == ' ') p++;
    if (strncmp(p, "moves", 5) == 0) {
        p += 5;
        while (*p) {
            while (*p == ' ') p++;
            if (*p == '\0') break;

            if (uci_parse_move(p, &m)) {
                board_make_move(m);
            }

            /* Skip this move token */
            while (*p && *p != ' ') p++;
        }
    }
}

static void uci_cmd_go(const char *line) {
    const char *p = line;
    u8 max_depth = 20;
    u32 max_time = 0;
    s32 wtime = -1, btime = -1;
    s32 winc = 0, binc = 0;
    u32 movetime = 0;
    SearchResult result;
    char move_str[6];

    /* Parse go parameters */
    while (*p) {
        while (*p == ' ') p++;

        if (strncmp(p, "depth", 5) == 0) {
            p += 5;
            while (*p == ' ') p++;
            max_depth = (u8)atoi(p);
            if (max_depth > MAX_PLY - 4) max_depth = MAX_PLY - 4;
        } else if (strncmp(p, "movetime", 8) == 0) {
            p += 8;
            while (*p == ' ') p++;
            movetime = (u32)atol(p);
        } else if (strncmp(p, "wtime", 5) == 0) {
            p += 5;
            while (*p == ' ') p++;
            wtime = atol(p);
        } else if (strncmp(p, "btime", 5) == 0) {
            p += 5;
            while (*p == ' ') p++;
            btime = atol(p);
        } else if (strncmp(p, "winc", 4) == 0) {
            p += 4;
            while (*p == ' ') p++;
            winc = atol(p);
        } else if (strncmp(p, "binc", 4) == 0) {
            p += 4;
            while (*p == ' ') p++;
            binc = atol(p);
        } else if (strncmp(p, "infinite", 8) == 0) {
            max_depth = MAX_PLY - 4;
            p += 8;
        }

        /* Skip to next token */
        while (*p && *p != ' ') p++;
    }

    /* Determine time for this move */
    if (movetime > 0) {
        max_time = movetime;
    } else if (wtime >= 0 || btime >= 0) {
        /* Simple time management: use 1/30th of remaining time + increment */
        s32 our_time = (g_state.side == WHITE) ? wtime : btime;
        s32 our_inc = (g_state.side == WHITE) ? winc : binc;
        if (our_time > 0) {
            max_time = (u32)(our_time / 30 + our_inc / 2);
            if (max_time > (u32)our_time - 100) {
                max_time = (u32)(our_time > 200 ? our_time - 100 : 100);
            }
        } else {
            max_time = 1000; /* fallback */
        }
    }

    /* Run search */
    result = search_position(max_depth, max_time);

    /* Output best move */
    if (result.best_move.from == 0 && result.best_move.to == 0) {
        printf("bestmove 0000\n");
    } else {
        uci_format_move(result.best_move, move_str);
        printf("bestmove %s\n", move_str);
    }
    fflush(stdout);
}

void uci_loop(void) {
    char line[4096];

    /* Disable output buffering for UCI compatibility */
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    board_init();
    tt_clear();

    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* Strip newline */
        {
            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                line[--len] = '\0';
            }
        }

        if (strcmp(line, "uci") == 0) {
            printf("id name %s\n", ENGINE_NAME);
            printf("id author %s\n", ENGINE_AUTHOR);
            printf("uciok\n");
            fflush(stdout);
        }
        else if (strcmp(line, "isready") == 0) {
            printf("readyok\n");
            fflush(stdout);
        }
        else if (strcmp(line, "ucinewgame") == 0) {
            board_init();
            tt_clear();
        }
        else if (strncmp(line, "position", 8) == 0) {
            uci_cmd_position(line + 8);
        }
        else if (strncmp(line, "go", 2) == 0) {
            uci_cmd_go(line + 2);
        }
        else if (strcmp(line, "quit") == 0) {
            break;
        }
        else if (strcmp(line, "d") == 0) {
            /* Debug: print board */
            board_print();
            fflush(stdout);
        }
#ifdef TARGET_TEST
        else if (strncmp(line, "perft", 5) == 0) {
            /* Used in test mode */
        }
#endif
    }
}

#endif /* !TARGET_C64 */
