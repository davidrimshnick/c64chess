#ifndef UCI_H
#define UCI_H

#include "../types.h"

/*
 * UCI (Universal Chess Interface) Protocol
 * For PC build only - enables testing with cutechess-cli and other GUIs.
 */

/* Run the UCI command loop (blocking, runs until 'quit') */
void uci_loop(void);

/* Parse a move string like "e2e4" or "e7e8q" and return a Move */
u8 uci_parse_move(const char *str, Move *m);

/* Format a move as UCI string (writes to buf, must be >= 6 bytes) */
void uci_format_move(Move m, char *buf);

#endif /* UCI_H */
