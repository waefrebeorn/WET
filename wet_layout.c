/*
 * wet_layout.c -- the panels: Stack, Dock, Canvas, Grid.
 *
 * Every panel runs the same two passes.  measure() asks each child for its
 * desired size and combines them; arrange() hands out the real rects.  A
 * panel never touches a control's pixels -- that separation is what lets a
 * .wet script reflow at any window size without moving a single widget by
 * hand.
 */
#include "wet_internal.h"

#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------- helpers */

int wet_panel_gap(const WetWidget *w) {
    (void)w;
    return 4;
}

/* Sum a child's margins, so a panel can add them without knowing the type. */
static int slot_h(const WetWidget *c) {
    return c->slot.margin_l + c->slot.margin_r;
}
static int slot_v(const WetWidget *c) {
    return c->slot.margin_t + c->slot.margin_b;
}

/* Desired size of one child, clamped to what is available. */
static WetSize measure_child(WetWidget *c, WetSize avail) {
    if (!c->visible) return (WetSize){0, 0};
    WetSize want = avail;
    if (want.w > 0) want.w -= slot_h(c);
    if (want.h > 0) want.h -= slot_v(c);
    c->cls->measure(c, want, &c->desired);
    WetSize d = c->desired;
    d.w += slot_h(c);
    d.h += slot_v(c);
    if (d.w < slot_h(c)) d.w = slot_h(c);
    if (d.h < slot_v(c)) d.h = slot_v(c);
    return d;
}

/* ----------------------------------------------------------------- Stack */

static void stack_init(WetWidget *w) {
    WetStack *s = (WetStack *)w->payload;
    s->orient = WET_ORIENT_VERTICAL;
    s->spacing = wet_panel_gap(w);   /* 0 is a real value: no gap */
}
static void stack_destroy(WetWidget *w)   { (void)w; }

static void stack_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    WetStack *s = (WetStack *)w->payload;
    int gap = s->spacing;
    int along = 0, across = 0;

    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        WetSize d = measure_child(c, avail);
        if (!c->visible) continue;
        if (s->orient == WET_ORIENT_VERTICAL) {
            along += d.h;
            if (d.w > across) across = d.w;
        } else {
            along += d.w;
            if (d.h > across) across = d.h;
        }
        if (i) along += gap;
    }
    *desired = (s->orient == WET_ORIENT_VERTICAL)
             ? (WetSize){ across, along }
             : (WetSize){ along, across };
}

static void stack_arrange(WetWidget *w, WetRect r) {
    WetStack *s = (WetStack *)w->payload;
    int gap = s->spacing;
    int pos = (s->orient == WET_ORIENT_VERTICAL) ? r.y : r.x;

    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (!c->visible) continue;
        WetSize d = c->desired;
        if (s->orient == WET_ORIENT_VERTICAL) {
            int x = r.x + c->slot.margin_l;
            int cw = r.w - slot_h(c);
            if (cw < 0) cw = 0;
            int ch = d.h - slot_v(c);
            if (ch < 0) ch = 0;
            if (s->expand || c->expand_v) ch = r.h - (pos - r.y) - slot_v(c);
            if (ch < 0) ch = 0;
            int y = pos + c->slot.margin_t;
            if (c->slot.valign == 1) y += (r.h - ch) / 2;
            if (c->slot.valign == 2) y += (r.h - ch) - slot_v(c);
            wet_widget_arrange(c, (WetRect){ x, y, cw, ch });
            pos += d.h + gap;
        } else {
            int y = r.y + c->slot.margin_t;
            int ch = r.h - slot_v(c);
            if (ch < 0) ch = 0;
            int cw = d.w - slot_h(c);
            if (cw < 0) cw = 0;
            if (s->expand || c->expand_h) cw = r.w - (pos - r.x) - slot_h(c);
            if (cw < 0) cw = 0;
            int x = pos + c->slot.margin_l;
            if (c->slot.halign == 1) x += (r.w - cw) / 2;
            if (c->slot.halign == 2) x += (r.w - cw) - slot_h(c);
            wet_widget_arrange(c, (WetRect){ x, y, cw, ch });
            pos += d.w + gap;
        }
    }
}

static void stack_render(WetWidget *w, WetRender *r) {
    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (c->visible) wet_widget_render(c, r);
    }
}

/* ------------------------------------------------------------------ Dock */

static void dock_init(WetWidget *w)    { (void)w; }
static void dock_destroy(WetWidget *w) { (void)w; }

static void dock_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    int left = 0, top = 0, right = 0, bottom = 0, fill_w = 0, fill_h = 0;
    int gap = wet_panel_gap(w);
    bool has_fill = false;

    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (!c->visible) continue;
        WetSize d = measure_child(c, avail);
        switch (c->slot.dock) {
        case WET_DOCK_LEFT:   left  += d.w + gap; break;
        case WET_DOCK_RIGHT:  right += d.w + gap; break;
        case WET_DOCK_TOP:    top   += d.h + gap; break;
        case WET_DOCK_BOTTOM: bottom+= d.h + gap; break;
        default:
            has_fill = true;
            if (d.w > fill_w) fill_w = d.w;
            if (d.h > fill_h) fill_h = d.h;
            break;
        }
    }
    *desired = (WetSize){ left + right + fill_w + (left||right?gap:0),
                           top + bottom + fill_h + (top||bottom?gap:0) };
    (void)has_fill;
}

static void dock_arrange(WetWidget *w, WetRect r) {
    int gap = wet_panel_gap(w);
    int left = r.x, top = r.y;
    int right = r.x + r.w, bottom = r.y + r.h;

    /* First pass: the four edges, in declaration order. */
    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (!c->visible || c->slot.dock == WET_DOCK_NONE || c->slot.dock == WET_DOCK_FILL) continue;
        WetSize d = c->desired;
        WetRect g = WET_RECT_EMPTY;
        switch (c->slot.dock) {
        case WET_DOCK_LEFT:
            g = (WetRect){ left + c->slot.margin_l, top + c->slot.margin_t,
                            d.w - slot_h(c), bottom - top - slot_v(c) };
            left += d.w + gap;
            break;
        case WET_DOCK_RIGHT:
            g = (WetRect){ right - d.w + c->slot.margin_l, top + c->slot.margin_t,
                            d.w - slot_h(c), bottom - top - slot_v(c) };
            right -= d.w + gap;
            break;
        case WET_DOCK_TOP:
            g = (WetRect){ left + c->slot.margin_l, top + c->slot.margin_t,
                            right - left - slot_h(c), d.h - slot_v(c) };
            top += d.h + gap;
            break;
        case WET_DOCK_BOTTOM:
            g = (WetRect){ left + c->slot.margin_l, bottom - d.h + c->slot.margin_t,
                            right - left - slot_h(c), d.h - slot_v(c) };
            bottom -= d.h + gap;
            break;
        default: break;
        }
        if (g.w < 0) g.w = 0;
        if (g.h < 0) g.h = 0;
        wet_widget_arrange(c, g);
    }
    /* Second pass: the fill child takes what is left.  A child with no dock
     * at all fills too -- that is the .NET default, and it is what makes
     * `wet.dock { children = { toolbar, body } }` work without ceremony. */
    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (!c->visible) continue;
        if (c->slot.dock != WET_DOCK_FILL && c->slot.dock != WET_DOCK_NONE) continue;
        wet_widget_arrange(c, (WetRect){ left + c->slot.margin_l, top + c->slot.margin_t,
                                         right - left - slot_h(c), bottom - top - slot_v(c) });
    }
}

static void dock_render(WetWidget *w, WetRender *r) {
    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (c->visible) wet_widget_render(c, r);
    }
}

/* ---------------------------------------------------------------- Canvas */

static void canvas_init(WetWidget *w)    { (void)w; }
static void canvas_destroy(WetWidget *w) { (void)w; }

static void canvas_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    int maxw = 0, maxh = 0;
    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (!c->visible) continue;
        WetSize d = measure_child(c, avail);
        int x2 = c->slot.canvas_left + d.w;
        int y2 = c->slot.canvas_top  + d.h;
        if (x2 > maxw) maxw = x2;
        if (y2 > maxh) maxh = y2;
    }
    *desired = (WetSize){ maxw, maxh };
}

static void canvas_arrange(WetWidget *w, WetRect r) {
    (void)r;
    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (!c->visible) continue;
        int cw, ch;
        if (c->slot.size_set) {
            cw = c->slot.size_w;
            ch = c->slot.size_h;
        } else {
            cw = c->desired.w - slot_h(c);
            ch = c->desired.h - slot_v(c);
        }
        if (cw < 0) cw = 0;
        if (ch < 0) ch = 0;
        wet_widget_arrange(c, (WetRect){ c->slot.canvas_left + c->slot.margin_l,
                                         c->slot.canvas_top  + c->slot.margin_t,
                                         cw, ch });
    }
}

static void canvas_render(WetWidget *w, WetRender *r) {
    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (c->visible) wet_widget_render(c, r);
    }
}

/* ------------------------------------------------------------------ Grid */

static void grid_init(WetWidget *w)    { ((WetGridPanel *)w->payload)->cols = 2; }
static void grid_destroy(WetWidget *w) { (void)w; }

static void grid_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    WetGridPanel *g = (WetGridPanel *)w->payload;
    int cols = g->cols > 0 ? g->cols : 1;
    int rows = g->rows > 0 ? g->rows : 1;
    int gap = wet_panel_gap(w);

    int *cw = (int *)calloc((size_t)cols, sizeof *cw);
    int *rh = (int *)calloc((size_t)rows, sizeof *rh);
    if (!cw || !rh) { free(cw); free(rh); *desired = (WetSize){0,0}; return; }

    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (!c->visible) continue;
        WetSize d = measure_child(c, avail);
        int col = c->slot.grid_col < 0 ? i % cols : c->slot.grid_col;
        int row = c->slot.grid_row < 0 ? i / cols : c->slot.grid_row;
        if (col < 0) col = 0;
        if (col >= cols) col = cols - 1;
        if (row < 0) row = 0;
        if (row >= rows) row = rows - 1;
        if (d.w > cw[col]) cw[col] = d.w;
        if (d.h > rh[row]) rh[row] = d.h;
    }
    int total_w = 0, total_h = 0;
    for (int i = 0; i < cols; i++) total_w += cw[i] + (i ? gap : 0);
    for (int i = 0; i < rows; i++) total_h += rh[i] + (i ? gap : 0);
    free(cw); free(rh);
    *desired = (WetSize){ total_w, total_h };
}

static void grid_arrange(WetWidget *w, WetRect r) {
    WetGridPanel *g = (WetGridPanel *)w->payload;
    int cols = g->cols > 0 ? g->cols : 1;
    int rows = g->rows > 0 ? g->rows : 1;
    int gap = wet_panel_gap(w);

    int *cw = (int *)calloc((size_t)cols, sizeof *cw);
    int *rh = (int *)calloc((size_t)rows, sizeof *rh);
    if (!cw || !rh) { free(cw); free(rh); return; }

    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (!c->visible) continue;
        int col = c->slot.grid_col < 0 ? i % cols : c->slot.grid_col;
        int row = c->slot.grid_row < 0 ? i / cols : c->slot.grid_row;
        if (col < 0) col = 0;
        if (col >= cols) col = cols - 1;
        if (row < 0) row = 0;
        if (row >= rows) row = rows - 1;
        if (c->desired.w > cw[col]) cw[col] = c->desired.w;
        if (c->desired.h > rh[row]) rh[row] = c->desired.h;
    }

    int *x = (int *)calloc((size_t)cols, sizeof *x);
    int *y = (int *)calloc((size_t)rows, sizeof *y);
    if (!x || !y) { free(cw); free(rh); free(x); free(y); return; }
    int acc = r.x;
    for (int i = 0; i < cols; i++) { x[i] = acc; acc += cw[i] + gap; }
    acc = r.y;
    for (int i = 0; i < rows; i++) { y[i] = acc; acc += rh[i] + gap; }

    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (!c->visible) continue;
        int col = c->slot.grid_col < 0 ? i % cols : c->slot.grid_col;
        int row = c->slot.grid_row < 0 ? i / cols : c->slot.grid_row;
        if (col < 0) col = 0;
        if (col >= cols) col = cols - 1;
        if (row < 0) row = 0;
        if (row >= rows) row = rows - 1;
        int cw2 = cw[col] - slot_h(c);
        int ch2 = rh[row] - slot_v(c);
        if (cw2 < 0) cw2 = 0;
        if (ch2 < 0) ch2 = 0;
        wet_widget_arrange(c, (WetRect){ x[col] + c->slot.margin_l,
                                         y[row] + c->slot.margin_t, cw2, ch2 });
    }
    free(cw); free(rh); free(x); free(y);
}

static void grid_render(WetWidget *w, WetRender *r) {
    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (c->visible) wet_widget_render(c, r);
    }
}

/* ------------------------------------------------------------- accessors */

WetOrient wet_stack_orient(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_STACK) ? ((WetStack *)w->payload)->orient
                                                : WET_ORIENT_VERTICAL;
}
void wet_stack_set_orient(WetWidget *w, WetOrient o) {
    if (w && w->cls->id == WET_CLASS_STACK) ((WetStack *)w->payload)->orient = o;
}
int  wet_stack_spacing(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_STACK) ? ((WetStack *)w->payload)->spacing : 0;
}
void wet_stack_set_spacing(WetWidget *w, int px) {
    if (w && w->cls->id == WET_CLASS_STACK) ((WetStack *)w->payload)->spacing = px;
}

WetDock wet_dock_get(const WetWidget *child) {
    return child ? child->slot.dock : WET_DOCK_NONE;
}
void wet_dock_set(WetWidget *child, WetDock dock) {
    if (child) child->slot.dock = dock;
}

int  wet_canvas_left(const WetWidget *child) { return child ? child->slot.canvas_left : 0; }
int  wet_canvas_top (const WetWidget *child) { return child ? child->slot.canvas_top  : 0; }
void wet_canvas_set_left(WetWidget *child, int x) { if (child) child->slot.canvas_left = x; }
void wet_canvas_set_top (WetWidget *child, int y) { if (child) child->slot.canvas_top  = y; }

/* An explicit designer size.  Without this the canvas sizes a control from its
 * own measured content, which is right for flowing layouts and wrong for a form
 * designer that places a button at exactly 100x28. */
void wet_canvas_set_size(WetWidget *child, int w, int h) {
    if (!child) return;
    child->slot.size_set = true;
    child->slot.size_w = w;
    child->slot.size_h = h;
}

int  wet_grid_col(const WetWidget *child) { return child ? child->slot.grid_col : 0; }
int  wet_grid_row(const WetWidget *child) { return child ? child->slot.grid_row : 0; }
void wet_grid_set_col(WetWidget *child, int c) { if (child) child->slot.grid_col = c; }
void wet_grid_set_row(WetWidget *child, int r) { if (child) child->slot.grid_row = r; }
int  wet_grid_columns(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_GRID) ? ((WetGridPanel *)w->payload)->cols : 0;
}
int  wet_grid_rows(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_GRID) ? ((WetGridPanel *)w->payload)->rows : 0;
}
void wet_grid_set_columns(WetWidget *w, int n) {
    if (w && w->cls->id == WET_CLASS_GRID && n > 0) ((WetGridPanel *)w->payload)->cols = n;
}
void wet_grid_set_rows(WetWidget *w, int n) {
    if (w && w->cls->id == WET_CLASS_GRID && n > 0) ((WetGridPanel *)w->payload)->rows = n;
}

/* ------------------------------------------------------------ registration */

void wet_layout_register(void) {
    static const WetWidgetClass stack_cls = {
        "stack", WET_CLASS_STACK, sizeof(WetStack),
        stack_init, stack_measure, stack_arrange, stack_render, NULL, stack_destroy
    };
    static const WetWidgetClass dock_cls = {
        "dock", WET_CLASS_DOCK, sizeof(WetDockPanel),
        dock_init, dock_measure, dock_arrange, dock_render, NULL, dock_destroy
    };
    static const WetWidgetClass canvas_cls = {
        "canvas", WET_CLASS_CANVAS, sizeof(WetCanvasPanel),
        canvas_init, canvas_measure, canvas_arrange, canvas_render, NULL, canvas_destroy
    };
    static const WetWidgetClass grid_cls = {
        "grid", WET_CLASS_GRID, sizeof(WetGridPanel),
        grid_init, grid_measure, grid_arrange, grid_render, NULL, grid_destroy
    };
    wet_class_register(&stack_cls);
    wet_class_register(&dock_cls);
    wet_class_register(&canvas_cls);
    wet_class_register(&grid_cls);
}
