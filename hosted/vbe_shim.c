/*
 * vbe_shim.c -- a headless stand-in for the WuBuOS VBE.
 *
 * The standalone repo builds and tests without a kernel.  This shim satisfies
 * vbe_interface.h and discards the drawing, so `make test` runs anywhere.  A
 * WuBuOS build links the real vbe.c instead and never sees this file.
 */
#include "vbe_interface.h"

#include <string.h>

void vbe_clear(uint32_t color) { (void)color; }
void vbe_fill_rect(int x, int y, int w, int h, uint32_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
void vbe_rect(int x, int y, int w, int h, uint32_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
void vbe_hline(int x1, int x2, int y, uint32_t color) {
    (void)x1; (void)x2; (void)y; (void)color;
}
void vbe_draw_text(int x, int y, const char *s, uint32_t color, int scale) {
    (void)x; (void)y; (void)s; (void)color; (void)scale;
}
int vbe_text_width(const char *s, int scale) {
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n * 6 * (scale > 0 ? scale : 1);
}
