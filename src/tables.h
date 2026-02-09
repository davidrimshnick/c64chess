#ifndef TABLES_H
#define TABLES_H

#include "types.h"

/*
 * Lookup Tables
 * - Zobrist random numbers for hashing
 * - Piece-square tables (from chessprogramming.org Simplified Evaluation)
 * - MVV-LVA scoring table
 * - Material values
 */

/* Material values indexed by piece type (0=empty,1=pawn,...,6=king) */
extern const s16 material_value[7];

/* Piece-square tables: [piece_type 1..6][square 0..63]
 * Values are from White's perspective, square 0 = a1
 * For Black, flip the square index: sq ^ 56 (or SQ_INDEX64(SQ_FLIP(sq)))
 */
extern const s8 pst_pawn[64];
extern const s8 pst_knight[64];
extern const s8 pst_bishop[64];
extern const s8 pst_rook[64];
extern const s8 pst_queen[64];
extern const s8 pst_king_mg[64];   /* middlegame king table */
extern const s8 pst_king_eg[64];   /* endgame king table */

/* Array of PST pointers indexed by piece type (0=NULL, 1=pawn, ..., 6=king_mg) */
extern const s8 *pst_table[7];

/* Zobrist random numbers - 16-bit on C64, 32-bit on PC */
#ifdef TARGET_C64
extern const u16 zobrist_pieces[2][7][128];  /* [color][piece_type][sq88] */
extern const u16 zobrist_side;               /* XOR when black to move */
extern const u16 zobrist_castle[16];         /* indexed by castle rights */
extern const u16 zobrist_ep[8];              /* indexed by file */
#else
extern const u32 zobrist_pieces[2][7][128];
extern const u32 zobrist_side;
extern const u32 zobrist_castle[16];
extern const u32 zobrist_ep[8];
#endif

/* MVV-LVA table: [victim_type][attacker_type] -> score */
extern const u8 mvv_lva[7][7];

/* Knight move offsets */
extern const s8 knight_offsets[8];

/* Bishop/Rook/Queen direction offsets */
extern const s8 bishop_offsets[4];
extern const s8 rook_offsets[4];

/* Castling rights update table: indexed by 0x88 square
 * castle_rights &= castle_mask[from] & castle_mask[to] */
extern const u8 castle_mask[128];

void tables_init(void);

#endif /* TABLES_H */
