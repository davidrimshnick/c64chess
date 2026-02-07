#ifndef EVAL_H
#define EVAL_H

#include "types.h"

/* Evaluate the current position from the side-to-move's perspective.
 * Uses incrementally-updated material and PST scores. */
s16 eval_position(void);

/* Check if position is likely an endgame (for king PST switching) */
u8 eval_is_endgame(void);

#endif /* EVAL_H */
