/*
 * vbe_interface.h -- the narrow VBE surface the .WET VBE backend needs.
 *
 * The framework never includes a kernel header.  A backend declares exactly
 * the drawing calls it uses, so WuBuOS's real vbe.h satisfies this by
 * inclusion and a hosted build satisfies it with a shim.  No second copy of
 * any drawing logic lives here -- only declarations.
 */
#ifndef WET_VBE_INTERFACE_H
#define WET_VBE_INTERFACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Colours are 0xRRGGBB words, matching WetColor. */
void vbe_clear(uint32_t color);
void vbe_fill_rect(int x, int y, int w, int h, uint32_t color);
void vbe_rect(int x, int y, int w, int h, uint32_t color);
void vbe_hline(int x1, int x2, int y, uint32_t color);
void vbe_draw_text(int x, int y, const char *s, uint32_t color, int scale);
int  vbe_text_width(const char *s, int scale);

#ifdef __cplusplus
}
#endif

#endif /* WET_VBE_INTERFACE_H */
