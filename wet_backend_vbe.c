/*
 * wet_backend_vbe.c -- the WuBuOS VBE backend for .WET.
 *
 * One backend, one job: turn the framework's WetRender calls into VBE calls.
 * The framework ships no drawing logic beyond this, so the same .wet applet
 * composites with the C Win98 shell instead of beside it.
 *
 * The VBE declarations come from vbe_interface.h; WuBuOS's real vbe.h
 * satisfies them, and a hosted build provides a shim.
 */
#include "wet_internal.h"

#include "vbe_interface.h"

static void vbe_clear_cb(void *ctx, WetColor c) {
    (void)ctx;
    vbe_clear((uint32_t)c);
}

static void vbe_fill_cb(void *ctx, int x, int y, int w, int h, WetColor c) {
    (void)ctx;
    vbe_fill_rect(x, y, w, h, (uint32_t)c);
}

static void vbe_rect_cb(void *ctx, int x, int y, int w, int h, WetColor c) {
    (void)ctx;
    vbe_rect(x, y, w, h, (uint32_t)c);
}

static void vbe_hline_cb(void *ctx, int x1, int x2, int y, WetColor c) {
    (void)ctx;
    vbe_hline(x1, x2, y, (uint32_t)c);
}

static void vbe_text_cb(void *ctx, int x, int y, const char *s, WetColor c) {
    (void)ctx;
    vbe_draw_text(x, y, s, (uint32_t)c, wet_theme_get()->font_scale);
}

static int vbe_text_width_cb(void *ctx, const char *s) {
    (void)ctx;
    return vbe_text_width(s, wet_theme_get()->font_scale);
}

static int vbe_text_height_cb(void *ctx) {
    (void)ctx;
    return wet_text_height();
}

WetRender *wet_render_vbe(void) {
    static WetRender r;
    r.ctx         = NULL;
    r.clear       = vbe_clear_cb;
    r.fill        = vbe_fill_cb;
    r.rect        = vbe_rect_cb;
    r.hline       = vbe_hline_cb;
    r.text        = vbe_text_cb;
    r.text_width  = vbe_text_width_cb;
    r.text_height = vbe_text_height_cb;
    return &r;
}
