#ifndef PLATFORM_H
#define PLATFORM_H

#include "../types.h"

/*
 * C64 Platform Abstraction
 * Direct hardware access for screen, keyboard, and timing.
 */

/* Screen RAM and Color RAM base addresses */
#define SCREEN_RAM  ((u8 *)0x0400)
#define COLOR_RAM   ((u8 *)0xD800)

/* Screen dimensions */
#define SCREEN_W 40
#define SCREEN_H 25

/* Colors */
#define COLOR_BLACK   0
#define COLOR_WHITE   1
#define COLOR_RED     2
#define COLOR_CYAN    3
#define COLOR_PURPLE  4
#define COLOR_GREEN   5
#define COLOR_BLUE    6
#define COLOR_YELLOW  7
#define COLOR_ORANGE  8
#define COLOR_BROWN   9
#define COLOR_LRED   10
#define COLOR_DGRAY  11
#define COLOR_MGRAY  12
#define COLOR_LGREEN 13
#define COLOR_LBLUE  14
#define COLOR_LGRAY  15

/* Initialize C64 platform (screen mode, colors, etc.) */
void platform_init(void);

/* Put a character at screen position (col, row) with color */
void platform_putc(u8 col, u8 row, u8 ch, u8 color);

/* Put a string at screen position */
void platform_puts(u8 col, u8 row, const char *str, u8 color);

/* Clear the screen */
void platform_cls(void);

/* Read a key (blocking) */
u8 platform_getkey(void);

/* Get elapsed time in jiffy clock ticks (1/60 sec) */
u32 platform_get_jiffies(void);

/* Bank out BASIC ROM to access RAM at $A000-$BFFF and $C000+ */
void platform_bank_out_basic(void);

#endif /* PLATFORM_H */
