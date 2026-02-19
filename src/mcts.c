#include "mcts.h"
#include "board.h"
#include "movegen.h"
#include "tables.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Node pool */
static MCTSNode node_pool[MCTS_MAX_NODES];
static u32 node_count;

static float exploration_c = 1.414f; /* sqrt(2) */

/* Simple xorshift32 PRNG */
static u32 rng_state = 12345;

static u32 xorshift32(void) {
    u32 x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

void mcts_set_exploration(float c) {
    exploration_c = c;
}

static u16 alloc_node(void) {
    if (node_count >= MCTS_MAX_NODES) return 0xFFFF;
    {
        u16 idx = (u16)node_count;
        MCTSNode *n = &node_pool[idx];
        memset(n, 0, sizeof(MCTSNode));
        n->parent = 0xFFFF;
        node_count++;
        return idx;
    }
}

/* UCT value for child selection */
static float uct_value(MCTSNode *parent, MCTSNode *child) {
    if (child->visits == 0) return 1e9f; /* unvisited = priority */
    {
        float exploit = child->wins / (float)child->visits;
        float explore = exploration_c * sqrtf(logf((float)parent->visits) / (float)child->visits);
        return exploit + explore;
    }
}

/* Select leaf node via UCT, returning its index. Replays moves on the board. */
static u16 select_leaf(u16 root_idx) {
    u16 current = root_idx;

    while (node_pool[current].expanded && node_pool[current].num_children > 0) {
        MCTSNode *node = &node_pool[current];
        float best_val = -1.0f;
        u16 best_child = 0;
        u16 i;

        for (i = 0; i < node->num_children; i++) {
            u16 ci = node->children[i];
            float val = uct_value(node, &node_pool[ci]);
            if (val > best_val) {
                best_val = val;
                best_child = ci;
            }
        }

        /* Make the move on the board */
        if (!board_make_move(node_pool[best_child].move)) {
            /* Illegal move (leaves king in check) - mark as loss */
            node_pool[best_child].visits += 1;
            node_pool[best_child].wins -= 1.0f;
            return current; /* stay at current */
        }
        current = best_child;
    }

    return current;
}

/* Expand a node: generate all legal moves as children */
static void expand(u16 node_idx) {
    MCTSNode *node = &node_pool[node_idx];
    u16 num_moves, base_idx, i;
    u8 ply = 0;

    if (node->expanded) return;
    node->expanded = 1;
    node->side = g_state.side;

    g_state.move_buf_idx[ply] = 0;
    num_moves = movegen_generate(ply);
    base_idx = g_state.move_buf_idx[ply];

    for (i = 0; i < num_moves && node->num_children < MCTS_MAX_CHILDREN; i++) {
        Move m = g_state.move_buf[base_idx + i];
        /* Check legality */
        if (board_make_move(m)) {
            board_unmake_move(m);
            {
                u16 child_idx = alloc_node();
                if (child_idx == 0xFFFF) break; /* out of nodes */
                node_pool[child_idx].move = m;
                node_pool[child_idx].parent = node_idx;
                node_pool[child_idx].side = g_state.side ^ 1; /* child is opponent's turn... wait no */
                node->children[node->num_children++] = child_idx;
            }
        }
    }
}

/* Random rollout from current position. Returns result from the
 * perspective of 'result_side': 1.0=win, 0.0=loss, 0.5=draw */
static float rollout(u8 result_side) {
    u16 depth = 0;
    Move rollout_moves[MCTS_MAX_ROLLOUT_PLY];
    u16 rollout_depth = 0;
    float result;

    while (depth < MCTS_MAX_ROLLOUT_PLY) {
        u16 num_moves, base_idx, legal_count;
        u16 picked;
        u8 ply = 0;
        u8 found = 0;

        /* Generate moves */
        g_state.move_buf_idx[ply] = 0;
        num_moves = movegen_generate(ply);
        if (num_moves == 0) {
            /* No pseudo-legal moves = stalemate or checkmate */
            if (board_in_check()) {
                /* Checkmate: current side lost */
                result = (g_state.side == result_side) ? 0.0f : 1.0f;
                goto rollout_done;
            } else {
                result = 0.5f;
                goto rollout_done;
            }
        }
        base_idx = g_state.move_buf_idx[ply];

        /* Pick a random legal move */
        /* First, count legal moves by trying them */
        legal_count = 0;
        {
            u16 j;
            for (j = 0; j < num_moves; j++) {
                Move m = g_state.move_buf[base_idx + j];
                if (board_make_move(m)) {
                    board_unmake_move(m);
                    legal_count++;
                }
            }
        }

        if (legal_count == 0) {
            if (board_in_check()) {
                result = (g_state.side == result_side) ? 0.0f : 1.0f;
            } else {
                result = 0.5f;
            }
            goto rollout_done;
        }

        /* Pick random legal move */
        picked = xorshift32() % legal_count;
        {
            u16 j, count = 0;
            for (j = 0; j < num_moves; j++) {
                Move m = g_state.move_buf[base_idx + j];
                if (board_make_move(m)) {
                    if (count == picked) {
                        rollout_moves[rollout_depth++] = m;
                        found = 1;
                        break;
                    }
                    board_unmake_move(m);
                    count++;
                }
            }
        }

        if (!found) break;

        /* Check draw conditions */
        if (g_state.fifty_clock >= 100 || board_is_repetition()) {
            result = 0.5f;
            goto rollout_done;
        }

        depth++;
    }

    /* Max depth reached - draw */
    result = 0.5f;

rollout_done:
    /* Undo all rollout moves */
    while (rollout_depth > 0) {
        rollout_depth--;
        board_unmake_move(rollout_moves[rollout_depth]);
    }

    return result;
}

/* Backpropagate result up the tree */
static void backpropagate(u16 node_idx, float result, u8 result_side) {
    u16 current = node_idx;
    while (current != 0xFFFF) {
        MCTSNode *node = &node_pool[current];
        node->visits++;
        /* Result is from result_side's perspective.
         * If this node's side-to-move == result_side, the result is
         * from *our* perspective. But UCT stores wins for the player
         * who *made* the move leading to this node, which is the opponent
         * of side-to-move. So we invert. */
        if (node->side == result_side) {
            node->wins += (1.0f - result);
        } else {
            node->wins += result;
        }
        current = node->parent;
    }
}

/* Save/restore board state for tree traversal */
typedef struct {
    GameState saved;
} BoardSnapshot;

static void snapshot_save(BoardSnapshot *snap) {
    memcpy(&snap->saved, &g_state, sizeof(GameState));
}

static void snapshot_restore(BoardSnapshot *snap) {
    memcpy(&g_state, &snap->saved, sizeof(GameState));
}

Move mcts_search(u32 num_simulations) {
    u16 root_idx;
    u32 sim;
    Move best_move;
    BoardSnapshot root_snap;

    best_move.from = 0;
    best_move.to = 0;
    best_move.flags = 0;
    best_move.score = 0;

    /* Reset node pool */
    node_count = 0;
    root_idx = alloc_node();
    if (root_idx == 0xFFFF) return best_move;

    /* Seed PRNG with something varying */
    rng_state = (u32)(g_state.hash) ^ (u32)num_simulations ^ 98765;

    /* Save root position */
    snapshot_save(&root_snap);

    /* Expand root */
    expand(root_idx);

    if (node_pool[root_idx].num_children == 0) {
        return best_move; /* no legal moves */
    }

    /* If only one legal move, return it immediately */
    if (node_pool[root_idx].num_children == 1) {
        return node_pool[node_pool[root_idx].children[0]].move;
    }

    for (sim = 0; sim < num_simulations; sim++) {
        u16 leaf;
        float result;
        u8 result_side;

        /* Restore root position */
        snapshot_restore(&root_snap);

        /* SELECT */
        leaf = select_leaf(root_idx);

        /* EXPAND (if not terminal and has visits) */
        if (!node_pool[leaf].expanded) {
            expand(leaf);
            /* Pick first unvisited child if available */
            if (node_pool[leaf].num_children > 0) {
                u16 ci = node_pool[leaf].children[0];
                if (!board_make_move(node_pool[ci].move)) {
                    /* shouldn't happen since we checked legality in expand */
                    continue;
                }
                leaf = ci;
            }
        }

        /* ROLLOUT */
        result_side = g_state.side; /* side to move at leaf */
        result = rollout(result_side);

        /* BACKPROPAGATE */
        backpropagate(leaf, result, result_side);
    }

    /* Pick move with most visits */
    {
        MCTSNode *root = &node_pool[root_idx];
        u32 best_visits = 0;
        u16 i;

        for (i = 0; i < root->num_children; i++) {
            MCTSNode *child = &node_pool[root->children[i]];
            if (child->visits > best_visits) {
                best_visits = child->visits;
                best_move = child->move;
            }
        }

        /* Debug output */
        fprintf(stderr, "MCTS: %u simulations, %u nodes\n",
                (unsigned)num_simulations, (unsigned)node_count);
        for (i = 0; i < root->num_children && i < 5; i++) {
            /* Show top 5 moves by visits */
            u32 top_visits = 0;
            u16 top_idx = 0, j;
            char from_str[3], to_str[3];
            for (j = 0; j < root->num_children; j++) {
                MCTSNode *c = &node_pool[root->children[j]];
                if (c->visits > top_visits) {
                    top_visits = c->visits;
                    top_idx = j;
                }
            }
            {
                MCTSNode *c = &node_pool[root->children[top_idx]];
                sq_to_str(c->move.from, from_str);
                sq_to_str(c->move.to, to_str);
                fprintf(stderr, "  %s%s: visits=%u wins=%.1f winrate=%.1f%%\n",
                        from_str, to_str, (unsigned)c->visits, c->wins,
                        c->visits > 0 ? 100.0f * c->wins / c->visits : 0.0f);
            }
            break; /* just show best for now */
        }
    }

    /* Restore root position */
    snapshot_restore(&root_snap);

    return best_move;
}
