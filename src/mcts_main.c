/*
 * MCTSlite UCI engine
 *
 * Pure MCTS with random rollouts, no evaluation function.
 * Configurable simulation count via UCI option "Simulations".
 * Used for Elo estimation of baseline MCTS strength.
 */

#ifndef TARGET_C64

#include "types.h"
#include "board.h"
#include "movegen.h"
#include "mcts.h"
#include "tables.h"
#include "tt.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static u32 num_simulations = 800;

/* Reuse uci_parse_move and uci_format_move from uci.c */
u8 uci_parse_move(const char *str, Move *m);
void uci_format_move(Move m, char *buf);

static void mcts_cmd_position(const char *line) {
    const char *p = line;
    Move m;

    while (*p == ' ') p++;

    if (strncmp(p, "startpos", 8) == 0) {
        board_init();
        p += 8;
    } else if (strncmp(p, "fen", 3) == 0) {
        p += 3;
        while (*p == ' ') p++;
        board_set_fen(p);
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

    while (*p == ' ') p++;
    if (strncmp(p, "moves", 5) == 0) {
        p += 5;
        while (*p) {
            while (*p == ' ') p++;
            if (*p == '\0') break;

            if (uci_parse_move(p, &m)) {
                board_make_move(m);
            }
            while (*p && *p != ' ') p++;
        }
    }
}

static void mcts_cmd_go(const char *line) {
    Move result;
    char move_str[6];
    u32 sims = num_simulations;

    /* Check for "go nodes N" to override sim count */
    {
        const char *p = line;
        while (*p) {
            while (*p == ' ') p++;
            if (strncmp(p, "nodes", 5) == 0) {
                p += 5;
                while (*p == ' ') p++;
                sims = (u32)atol(p);
            }
            while (*p && *p != ' ') p++;
        }
    }

    /* If sims==0, just pick a random legal move */
    if (sims == 0) {
        u16 num_moves, base_idx, legal_count, picked, count;
        u16 i;
        u8 ply = 0;

        g_state.move_buf_idx[ply] = 0;
        num_moves = movegen_generate(ply);
        base_idx = g_state.move_buf_idx[ply];

        legal_count = 0;
        for (i = 0; i < num_moves; i++) {
            Move m = g_state.move_buf[base_idx + i];
            if (board_make_move(m)) {
                board_unmake_move(m);
                legal_count++;
            }
        }

        if (legal_count == 0) {
            printf("bestmove 0000\n");
            fflush(stdout);
            return;
        }

        picked = (u16)(rand() % legal_count);
        count = 0;
        for (i = 0; i < num_moves; i++) {
            Move m = g_state.move_buf[base_idx + i];
            if (board_make_move(m)) {
                if (count == picked) {
                    board_unmake_move(m);
                    uci_format_move(m, move_str);
                    printf("info string random move\n");
                    printf("bestmove %s\n", move_str);
                    fflush(stdout);
                    return;
                }
                board_unmake_move(m);
                count++;
            }
        }

        printf("bestmove 0000\n");
        fflush(stdout);
        return;
    }

    result = mcts_search(sims);

    if (result.from == 0 && result.to == 0) {
        printf("bestmove 0000\n");
    } else {
        uci_format_move(result, move_str);
        printf("info string sims %u\n", (unsigned)sims);
        printf("bestmove %s\n", move_str);
    }
    fflush(stdout);
}

static void mcts_uci_loop(void) {
    char line[4096];

    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    board_init();

    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        if (strcmp(line, "uci") == 0) {
            printf("id name MCTSlite\n");
            printf("id author C64Chess\n");
            printf("option name Simulations type spin default 800 min 0 max 100000\n");
            printf("option name Seed type spin default 0 min 0 max 2147483647\n");
            printf("uciok\n");
            fflush(stdout);
        }
        else if (strcmp(line, "isready") == 0) {
            printf("readyok\n");
            fflush(stdout);
        }
        else if (strcmp(line, "ucinewgame") == 0) {
            board_init();
        }
        else if (strncmp(line, "setoption", 9) == 0) {
            /* Parse "setoption name Simulations value N" */
            const char *p = strstr(line, "value");
            if (p) {
                p += 5;
                while (*p == ' ') p++;
                if (strstr(line, "Simulations")) {
                    num_simulations = (u32)atol(p);
                    fprintf(stderr, "MCTSlite: simulations = %u\n", (unsigned)num_simulations);
                } else if (strstr(line, "Seed")) {
                    mcts_set_seed((u32)atol(p));
                    fprintf(stderr, "MCTSlite: seed = %u\n", (unsigned)atol(p));
                }
            }
        }
        else if (strncmp(line, "position", 8) == 0) {
            mcts_cmd_position(line + 8);
        }
        else if (strncmp(line, "go", 2) == 0) {
            mcts_cmd_go(line + 2);
        }
        else if (strcmp(line, "quit") == 0) {
            break;
        }
    }
}

int main(void) {
    tables_init();
    mcts_uci_loop();
    return 0;
}

#endif /* !TARGET_C64 */
