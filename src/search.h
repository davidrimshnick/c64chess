#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

/*
 * Search Engine
 * - Iterative deepening
 * - Negamax with alpha-beta pruning
 * - Quiescence search
 * - Null move pruning
 * - Late move reductions
 * - Transposition table
 */

/* Search result */
typedef struct {
    Move best_move;
    s16  score;
    u8   depth;
    u32  nodes;
} SearchResult;

/* Search info (for UCI info output) */
typedef struct {
    u32  nodes;
    u8   max_depth;     /* max depth to search */
    u32  max_time_ms;   /* max time in milliseconds (0 = no limit) */
    u32  start_time;    /* search start timestamp */
    u8   stopped;       /* set to 1 to abort search */
    u8   use_time;      /* 1 if time control is active */
} SearchInfo;

extern SearchInfo g_search_info;

/* Run iterative deepening search. Returns best move and score. */
SearchResult search_position(u8 max_depth, u32 max_time_ms);

/* Get current time in milliseconds (platform-specific) */
u32 get_time_ms(void);

/* Check if search should be stopped (time limit reached) */
void search_check_time(void);

#endif /* SEARCH_H */
