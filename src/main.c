/*
 * C64 Chess Engine
 * Main entry point - dispatches to C64 UI or UCI protocol
 */

#include "types.h"
#include "board.h"
#include "tables.h"
#include "tt.h"

#ifdef TARGET_C64
#include "c64/ui.h"
#include "c64/platform.h"
#else
#include "uci/uci.h"
#endif

int main(void) {
    tables_init();

#ifdef TARGET_C64
    ui_game_loop();
#else
    uci_loop();
#endif

    return 0;
}
