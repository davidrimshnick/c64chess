#ifndef MOVESORT_H
#define MOVESORT_H

#include "types.h"

/*
 * Move Ordering
 * Assigns scores to moves for alpha-beta search efficiency.
 * Order: PV move > Captures (MVV-LVA) > Killers > Quiet moves
 */

/* Score all moves at a given ply for ordering */
void movesort_score_moves(u8 ply, u16 num_moves, Move *pv_move);

/* Pick the best-scored move from position idx onward (selection sort step).
 * Swaps it into position idx. */
void movesort_pick_best(u8 ply, u16 idx, u16 num_moves);

/* Update killer move table after a beta cutoff */
void movesort_update_killers(u8 ply, Move m);

/* Clear killer move table */
void movesort_clear_killers(void);

#endif /* MOVESORT_H */
