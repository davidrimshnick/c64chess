#ifdef TARGET_C64

#include "platform.h"
#include <conio.h>
#include <c64.h>

void platform_init(void) {
    /* Bank out BASIC ROM to use RAM underneath */
    platform_bank_out_basic();

    /* Set border and background colors */
    VIC.bordercolor = COLOR_DGRAY;
    VIC.bgcolor0 = COLOR_BLACK;

    /* Switch to uppercase/lowercase character set */
    /* Poke 53272 with (peek(53272) & 0xF1) | 0x06 for lowercase */
    /* Actually keep uppercase PETSCII for piece characters */

    /* Clear screen */
    platform_cls();
}

void platform_putc(u8 col, u8 row, u8 ch, u8 color) {
    u16 offset = (u16)row * 40 + col;
    SCREEN_RAM[offset] = ch;
    COLOR_RAM[offset] = color;
}

void platform_puts(u8 col, u8 row, const char *str, u8 color) {
    u16 offset = (u16)row * 40 + col;
    while (*str) {
        SCREEN_RAM[offset] = *str;
        COLOR_RAM[offset] = color;
        str++;
        offset++;
    }
}

void platform_cls(void) {
    u16 i;
    for (i = 0; i < 1000; i++) {
        SCREEN_RAM[i] = 0x20; /* space */
        COLOR_RAM[i] = COLOR_LGRAY;
    }
}

u8 platform_getkey(void) {
    return (u8)cgetc();
}

u32 platform_get_jiffies(void) {
    u32 j;
    j  = (u32)(*(u8 *)0xA0) << 16;
    j |= (u32)(*(u8 *)0xA1) << 8;
    j |= (u32)(*(u8 *)0xA2);
    return j;
}

void platform_bank_out_basic(void) {
    /* Memory configuration register at $0001
     * Bit 0: LORAM (BASIC ROM at $A000-$BFFF)
     * Bit 1: HIRAM (Kernal ROM at $E000-$FFFF)
     * Bit 2: CHAREN (Character ROM / I/O)
     * Default: 0x37 (all ROMs visible)
     * We want: 0x36 (BASIC ROM replaced by RAM, keep Kernal + I/O) */
    *(u8 *)0x0001 = 0x36;
}

#endif /* TARGET_C64 */
