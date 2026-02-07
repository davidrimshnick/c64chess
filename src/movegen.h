#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "types.h"

/*
 * Move Generation
 *
 * Generates pseudo-legal moves into the global move buffer.
 * Legality is checked later in board_make_move (king not left in check).
 *
 * Uses a flat buffer with ply-based indexing to avoid dynamic allocation.
 */

/* Generate all pseudo-legal moves for the current position.
 * Writes to g_state.move_buf starting at g_state.move_buf_idx[ply].
 * Returns the number of moves generated. */
u16 movegen_generate(u8 ply);

/* Generate only capture and promotion moves (for quiescence search).
 * Returns the number of moves generated. */
u16 movegen_generate_captures(u8 ply);

/* Check if the current side has any legal moves (for mate/stalemate detection) */
u8 movegen_has_legal_move(void);

#endif /* MOVEGEN_H */
