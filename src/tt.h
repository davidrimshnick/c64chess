#ifndef TT_H
#define TT_H

#include "types.h"

/*
 * Transposition Table
 * 512 entries x 8 bytes = 4KB
 * On C64: mapped to $C000 via custom linker segment
 * On PC: normal static array
 */

/* Initialize/clear the transposition table */
void tt_clear(void);

/* Probe the TT for the current position.
 * Returns 1 if hit found, fills score/best_move/depth/flag.
 * Adjusts mate scores for ply distance. */
u8 tt_probe(HashKey hash, u8 depth, s16 alpha, s16 beta,
            s16 *score, Move *best_move, u8 search_ply);

/* Store a position in the TT.
 * flag: TT_FLAG_EXACT, TT_FLAG_ALPHA, or TT_FLAG_BETA */
void tt_store(HashKey hash, u8 depth, s16 score, u8 flag,
              Move best_move, u8 search_ply);

/* Probe for best move only (for PV extraction) */
u8 tt_probe_move(HashKey hash, Move *best_move);

#endif /* TT_H */
