#ifndef BOARD_H
#define BOARD_H

#include "types.h"

/* Initialize board to starting position */
void board_init(void);

/* Set board from FEN string. Returns 1 on success, 0 on failure. */
u8 board_set_fen(const char *fen);

/* Convert board to FEN string (writes to buf, must be >= 90 bytes) */
void board_get_fen(char *buf);

/* Make a move on the board. Returns 1 if legal (king not in check after). */
u8 board_make_move(Move m);

/* Unmake the last move */
void board_unmake_move(Move m);

/* Make/unmake null move (pass turn without moving) */
void board_make_null(void);
void board_unmake_null(void);

/* Check if a square is attacked by the given side */
u8 board_is_square_attacked(u8 sq, u8 by_side);

/* Check if current side's king is in check */
u8 board_in_check(void);

/* Check for repetition (threefold) */
u8 board_is_repetition(void);

/* Compute Zobrist hash from scratch (for verification) */
HashKey board_compute_hash(void);

/* Print board to stdout (for debugging on PC) */
void board_print(void);

/* Square name helpers */
char file_to_char(u8 file);
char rank_to_char(u8 rank);
void sq_to_str(u8 sq, char *buf);

#endif /* BOARD_H */
