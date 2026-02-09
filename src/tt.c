#include "tt.h"

/*
 * On C64: place TT in the TTABLE segment at $C000 (banked-out BASIC ROM).
 * On PC: just a regular static array.
 */
#ifdef TARGET_C64
#pragma bss-name("TTABLE")
#endif

static TTEntry tt_table[TT_SIZE];

#ifdef TARGET_C64
#pragma bss-name(push, "BSS")
#endif

/* Get TT index from hash */
static u16 tt_index(HashKey hash) {
    return (u16)(hash & (TT_SIZE - 1));  /* TT_SIZE must be power of 2 */
}

/* Adjust mate scores for storage (make them relative to root, not ply) */
static s16 score_to_tt(s16 score, u8 ply) {
    if (score > SCORE_MATE - 100) return score + ply;
    if (score < SCORE_MATED + 100) return score - ply;
    return score;
}

static s16 score_from_tt(s16 score, u8 ply) {
    if (score > SCORE_MATE - 100) return score - ply;
    if (score < SCORE_MATED + 100) return score + ply;
    return score;
}

void tt_clear(void) {
    u32 i;
    for (i = 0; i < TT_SIZE; i++) {
        tt_table[i].key = 0;
        tt_table[i].score = 0;
        tt_table[i].best.from = 0;
        tt_table[i].best.to = 0;
        tt_table[i].best.flags = 0;
        tt_table[i].best.score = 0;
        tt_table[i].depth = 0;
    }
}

u8 tt_probe(HashKey hash, u8 depth, s16 alpha, s16 beta,
            s16 *score, Move *best_move, u8 search_ply) {
    u16 idx = tt_index(hash);
    TTEntry *entry = &tt_table[idx];
    u8 tt_depth, tt_flag;

    /* Check key match */
    if (entry->key != TT_KEY(hash)) return 0;

    /* Always extract best move if available */
    if (best_move) {
        *best_move = entry->best;
    }

    /* Check depth */
    tt_depth = entry->depth & 0x3F;  /* lower 6 bits = depth */
    tt_flag = (entry->depth >> 6) & 0x03; /* upper 2 bits = flag */

    if (tt_depth < depth) return 0;

    /* Adjust score for mate distance */
    {
        s16 tt_score = score_from_tt(entry->score, search_ply);

        switch (tt_flag) {
            case TT_FLAG_EXACT:
                *score = tt_score;
                return 1;
            case TT_FLAG_ALPHA:
                if (tt_score <= alpha) {
                    *score = alpha;
                    return 1;
                }
                break;
            case TT_FLAG_BETA:
                if (tt_score >= beta) {
                    *score = beta;
                    return 1;
                }
                break;
        }
    }

    return 0;
}

void tt_store(HashKey hash, u8 depth, s16 score, u8 flag,
              Move best_move, u8 search_ply) {
    u16 idx = tt_index(hash);
    TTEntry *entry = &tt_table[idx];

    /* Always-replace scheme (simple, works well with small TT) */
    entry->key = TT_KEY(hash);
    entry->score = score_to_tt(score, search_ply);
    entry->best = best_move;
    entry->best.score = 0; /* don't store move ordering score */
    entry->depth = (u8)((depth & 0x3F) | (flag << 6));
}

u8 tt_probe_move(HashKey hash, Move *best_move) {
    u16 idx = tt_index(hash);
    TTEntry *entry = &tt_table[idx];

    if (entry->key != TT_KEY(hash)) return 0;
    if (entry->best.from == 0 && entry->best.to == 0) return 0;

    *best_move = entry->best;
    return 1;
}
