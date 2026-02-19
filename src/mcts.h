#ifndef MCTS_H
#define MCTS_H

#include "types.h"

/*
 * Pure MCTS (Monte Carlo Tree Search) - "MCTSlite"
 *
 * No evaluation function, no policy prior.
 * Uses random rollouts to estimate move values.
 * UCT selection with C = sqrt(2).
 */

#define MCTS_MAX_NODES    100000
#define MCTS_MAX_CHILDREN MAX_MOVES
#define MCTS_MAX_ROLLOUT_PLY 200

typedef struct MCTSNode {
    Move move;              /* move that led to this node */
    u32 visits;
    float wins;             /* wins from this node's perspective */
    u16 parent;             /* index into node pool (0xFFFF = none) */
    u16 children[MCTS_MAX_CHILDREN];
    u16 num_children;
    u8  expanded;           /* 1 if children have been generated */
    u8  side;               /* side to move at this node */
} MCTSNode;

/* Run MCTS search with given number of simulations. Returns best move. */
Move mcts_search(u32 num_simulations);

/* Set exploration constant (default sqrt(2)) */
void mcts_set_exploration(float c);

#endif /* MCTS_H */
