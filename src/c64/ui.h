#ifndef UI_H
#define UI_H

#include "../types.h"

/*
 * C64 Chess User Interface
 * PETSCII text-mode chess board with keyboard input.
 */

/* Run the C64 game loop */
void ui_game_loop(void);

/* Draw the chess board */
void ui_draw_board(void);

/* Display a message on the status line */
void ui_status(const char *msg);

/* Get a move from the human player (returns move in algebraic "e2e4" format) */
u8 ui_get_move(Move *m);

/* Display search info during computer thinking */
void ui_show_thinking(u8 depth, s16 score);

#endif /* UI_H */
