/*
 * wet_render.c -- the render primitives.
 *
 * Every control draws through these, and they clip against the current bounds
 * so a control cannot paint over its neighbours.  A backend (wet_backend_vbe.c
 * for WuBuOS) supplies the WetRender function table; a test supplies its own
 * and gets a log instead of pixels.
 */
#include "wet_internal.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------ primitives */

void wet_r_fill(WetRender *r, WetRect bounds, WetRect g, WetColor c) {
    if (!r || !r->fill) return;
    WetRect c2 = wet_rect_clip(g, bounds);
    if (c2.w <= 0 || c2.h <= 0) return;
    r->fill(r->ctx, c2.x, c2.y, c2.w, c2.h, c);
}

void wet_r_rect(WetRender *r, WetRect bounds, WetRect g, WetColor c) {
    if (!r || !r->rect) return;
    WetRect c2 = wet_rect_clip(g, bounds);
    if (c2.w <= 0 || c2.h <= 0) return;
    r->rect(r->ctx, c2.x, c2.y, c2.w, c2.h, c);
}

void wet_r_text(WetRender *r, WetRect bounds, int x, int y, const char *s, WetColor c) {
    if (!r || !r->text || !s || !*s) return;
    if (x >= bounds.x + bounds.w || y >= bounds.y + bounds.h) return;
    if (y + wet_text_height() <= bounds.y || x + wet_text_width(s) <= bounds.x) return;
    r->text(r->ctx, x, y, s, c);
}

/* A classic Win9x raised face: lit top/left, shaded bottom/right. */
void wet_r_raised(WetRender *r, WetRect bounds, WetRect g, WetColor face) {
    const WetTheme *t = wet_theme_get();
    if (g.w <= 0 || g.h <= 0) return;
    wet_r_fill(r, bounds, g, face);
    wet_r_rect(r, bounds, (WetRect){g.x, g.y, g.w, 1}, t->border_lit);
    wet_r_rect(r, bounds, (WetRect){g.x, g.y, 1, g.h}, t->border_lit);
    wet_r_rect(r, bounds, (WetRect){g.x, g.y + g.h - 1, g.w, 1}, t->border_shade);
    wet_r_rect(r, bounds, (WetRect){g.x + g.w - 1, g.y, 1, g.h}, t->border_shade);
}

void wet_r_sunken(WetRender *r, WetRect bounds, WetRect g, WetColor face) {
    const WetTheme *t = wet_theme_get();
    if (g.w <= 0 || g.h <= 0) return;
    wet_r_fill(r, bounds, g, face);
    wet_r_rect(r, bounds, (WetRect){g.x, g.y, g.w, 1}, t->border_shade);
    wet_r_rect(r, bounds, (WetRect){g.x, g.y, 1, g.h}, t->border_shade);
    wet_r_rect(r, bounds, (WetRect){g.x, g.y + g.h - 1, g.w, 1}, t->border_lit);
    wet_r_rect(r, bounds, (WetRect){g.x + g.w - 1, g.y, 1, g.h}, t->border_lit);
}
