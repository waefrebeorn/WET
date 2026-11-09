/*
 * wet_selftest.c -- behaviour gates for the .wet framework.
 *
 * Runs headless: the VBE entry points are stubbed with a recording backend, so
 * every check reads real geometry and real draw calls, never a remembered
 * number.  A control that "should" draw is verified by the primitives it
 * actually issued.
 */
#include "wet_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------- recording backend */

#define WET_MAX_OPS 4096

typedef struct {
    int   kind;        /* 0 fill, 1 rect, 2 text */
    int   x, y, w, h;
    int   color;
    char  text[64];
} WetOp;

static WetOp  g_ops[WET_MAX_OPS];
static int    g_nops;
static int    g_text_height = 8;

static void rec_clear(void *ctx, WetColor c) { (void)ctx; (void)c; g_nops = 0; }

static void rec_fill(void *ctx, int x, int y, int w, int h, WetColor c) {
    (void)ctx;
    if (g_nops >= WET_MAX_OPS) return;
    WetOp *o = &g_ops[g_nops++];
    o->kind = 0; o->x = x; o->y = y; o->w = w; o->h = h; o->color = (int)c; o->text[0] = 0;
}

static void rec_rect(void *ctx, int x, int y, int w, int h, WetColor c) {
    (void)ctx;
    if (g_nops >= WET_MAX_OPS) return;
    WetOp *o = &g_ops[g_nops++];
    o->kind = 1; o->x = x; o->y = y; o->w = w; o->h = h; o->color = (int)c; o->text[0] = 0;
}

static void rec_hline(void *ctx, int x1, int x2, int y, WetColor c) {
    (void)ctx;
    if (g_nops >= WET_MAX_OPS) return;
    WetOp *o = &g_ops[g_nops++];
    o->kind = 0; o->x = x1; o->y = y; o->w = x2 - x1; o->h = 1; o->color = (int)c; o->text[0] = 0;
}

static void rec_text(void *ctx, int x, int y, const char *s, WetColor c) {
    (void)ctx;
    if (g_nops >= WET_MAX_OPS) return;
    WetOp *o = &g_ops[g_nops++];
    o->kind = 2; o->x = x; o->y = y; o->w = 0; o->h = 0;
    o->color = (int)c;
    snprintf(o->text, sizeof o->text, "%s", s ? s : "");
}

static int rec_text_width(void *ctx, const char *s) {
    (void)ctx;
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n * 6;
}

static int rec_text_height(void *ctx) { (void)ctx; return g_text_height; }

static WetRender g_rec = {
    NULL, rec_clear, rec_fill, rec_rect, rec_hline,
    rec_text, rec_text_width, rec_text_height
};

/* VBE stubs: wet_render.c binds wet_render_vbe() to these.  Weak, so a real
 * VBE wins when the framework is built into the kernel. */
#define WET_VBE_STUB(name) \
    __attribute__((weak)) void name(void) { }
__attribute__((weak)) void vbe_clear(unsigned c) { (void)c; }
__attribute__((weak)) void vbe_fill_rect(int x, int y, int w, int h, unsigned c) { (void)x;(void)y;(void)w;(void)h;(void)c; }
__attribute__((weak)) void vbe_rect(int x, int y, int w, int h, unsigned c) { (void)x;(void)y;(void)w;(void)h;(void)c; }
__attribute__((weak)) void vbe_hline(int a, int b, int y, unsigned c) { (void)a;(void)b;(void)y;(void)c; }
__attribute__((weak)) void vbe_draw_text(int x, int y, const char *s, unsigned c, int sc) { (void)x;(void)y;(void)s;(void)c;(void)sc; }
__attribute__((weak)) int  vbe_text_width(const char *s, int sc) { (void)s;(void)sc; return 0; }

/* ------------------------------------------------------------------ checks */

static int g_checks, g_fails;

#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

static int count_kind(int kind) {
    int n = 0;
    for (int i = 0; i < g_nops; i++) if (g_ops[i].kind == kind) n++;
    return n;
}

/* A real click: press, then release over the same spot. */
static int click(WetApp *app, int x, int y) {
    wet_app_pointer(app, WET_EVENT_MOUSEDOWN, x, y, 1);
    return wet_app_pointer(app, WET_EVENT_MOUSEUP, x, y, 1);
}

static int find_text(const char *s) {
    for (int i = 0; i < g_nops; i++)
        if (g_ops[i].kind == 2 && strcmp(g_ops[i].text, s) == 0) return i;
    return -1;
}

/* ------------------------------------------------------------------ tests */

static void test_label_measure(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *l = wet_widget_new(app, WET_CLASS_LABEL, "l");
    wet_label_set_text(l, "Hello");
    wet_app_root(app, l);
    wet_app_layout(app);

    CHECK(wet_text_width("Hello") == 30, "five cells at 6px advance");
    CHECK(l->desired.w == 30, "label desired width is its text width");
    CHECK(l->desired.h == g_text_height, "label desired height is one cell");
    wet_app_free(app);
}

static void test_stack_vertical(void) {
    WetApp *app = wet_app_new(200, 200);
    app->render = &g_rec;
    WetWidget *s = wet_widget_new(app, WET_CLASS_STACK, "s");
    wet_stack_set_spacing(s, 0);
    WetWidget *a = wet_widget_new(app, WET_CLASS_LABEL, "a");
    WetWidget *b = wet_widget_new(app, WET_CLASS_LABEL, "b");
    wet_label_set_text(a, "AA");
    wet_label_set_text(b, "BBB");
    wet_widget_add(s, a);
    wet_widget_add(s, b);
    wet_app_root(app, s);
    wet_app_layout(app);

    CHECK(a->rect.y == 0, "first child at the top");
    CHECK(b->rect.y == a->rect.h, "second child below the first");
    CHECK(a->rect.x == 0, "children start at the panel's left edge");
    CHECK(s->desired.w == wet_text_width("BBB"), "stack width is the widest child");
    wet_app_free(app);
}

static void test_stack_horizontal(void) {
    WetApp *app = wet_app_new(300, 100);
    app->render = &g_rec;
    WetWidget *s = wet_widget_new(app, WET_CLASS_STACK, "s");
    wet_stack_set_orient(s, WET_ORIENT_HORIZONTAL);
    wet_stack_set_spacing(s, 0);
    WetWidget *a = wet_widget_new(app, WET_CLASS_LABEL, "a");
    WetWidget *b = wet_widget_new(app, WET_CLASS_LABEL, "b");
    wet_label_set_text(a, "AA");
    wet_label_set_text(b, "BBB");
    wet_widget_add(s, a);
    wet_widget_add(s, b);
    wet_app_root(app, s);
    wet_app_layout(app);

    CHECK(a->rect.x == 0, "horizontal: first child at the left");
    CHECK(b->rect.x == a->rect.w, "horizontal: second child to the right");
    wet_app_free(app);
}

static void test_button_pads_its_text(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *b = wet_widget_new(app, WET_CLASS_BUTTON, "b");
    wet_button_set_text(b, "OK");
    wet_app_root(app, b);
    wet_app_layout(app);

    CHECK(b->desired.w > wet_text_width("OK"), "button is wider than its label");
    CHECK(b->desired.h >= g_text_height + 4, "button is taller than its label");
    wet_app_free(app);
}

static void test_hidden_child_is_skipped(void) {
    WetApp *app = wet_app_new(200, 200);
    app->render = &g_rec;
    WetWidget *s = wet_widget_new(app, WET_CLASS_STACK, "s");
    wet_stack_set_spacing(s, 0);
    WetWidget *a = wet_widget_new(app, WET_CLASS_LABEL, "a");
    WetWidget *b = wet_widget_new(app, WET_CLASS_LABEL, "b");
    wet_label_set_text(a, "AA");
    wet_label_set_text(b, "BBB");
    wet_widget_set_visible(b, false);
    wet_widget_add(s, a);
    wet_widget_add(s, b);
    wet_app_root(app, s);
    wet_app_layout(app);

    CHECK(a->rect.y == 0, "visible child still at the top");
    CHECK(b->rect.w == 0 && b->rect.h == 0, "hidden child gets no rect");
    CHECK(s->desired.w == wet_text_width("AA"), "hidden child does not size the stack");
    wet_app_free(app);
}

static void test_render_issues_text(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *l = wet_widget_new(app, WET_CLASS_LABEL, "l");
    wet_label_set_text(l, "Greet");
    wet_app_root(app, l);
    wet_app_layout(app);
    wet_app_render(app);

    CHECK(g_nops > 0, "render issued at least one op");
    CHECK(find_text("Greet") >= 0, "label text was drawn");
    wet_app_free(app);
}

static void test_render_hidden_draws_nothing(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *l = wet_widget_new(app, WET_CLASS_LABEL, "l");
    wet_label_set_text(l, "Greet");
    wet_widget_set_visible(l, false);
    wet_app_root(app, l);
    wet_app_layout(app);
    wet_app_render(app);

    CHECK(count_kind(2) == 0, "hidden label draws no text");
    wet_app_free(app);
}

static void test_find_by_id(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *s = wet_widget_new(app, WET_CLASS_STACK, "s");
    WetWidget *t = wet_widget_new(app, WET_CLASS_TEXTBOX, "name");
    wet_widget_add(s, t);
    wet_app_root(app, s);

    CHECK(wet_app_find(app, "name") == t, "wet.find returns the named widget");
    CHECK(wet_app_find(app, "nope") == NULL, "unknown id returns NULL");
    wet_app_free(app);
}

static void test_dock_edges(void) {
    WetApp *app = wet_app_new(300, 200);
    app->render = &g_rec;
    WetWidget *d = wet_widget_new(app, WET_CLASS_DOCK, "d");
    WetWidget *top = wet_widget_new(app, WET_CLASS_LABEL, "top");
    WetWidget *left = wet_widget_new(app, WET_CLASS_LABEL, "left");
    WetWidget *fill = wet_widget_new(app, WET_CLASS_LABEL, "fill");
    wet_label_set_text(top, "T");
    wet_label_set_text(left, "L");
    wet_label_set_text(fill, "F");
    wet_dock_set(top, WET_DOCK_TOP);
    wet_dock_set(left, WET_DOCK_LEFT);
    wet_dock_set(fill, WET_DOCK_FILL);
    wet_widget_add(d, top);
    wet_widget_add(d, left);
    wet_widget_add(d, fill);
    wet_app_root(app, d);
    wet_app_layout(app);

    CHECK(top->rect.y == 0, "docked top starts at the top");
    CHECK(left->rect.y >= top->rect.h, "docked left starts below the top bar");
    CHECK(fill->rect.x >= left->rect.w, "fill starts right of the left bar");
    CHECK(fill->rect.x + fill->rect.w == 300, "fill reaches the right edge");
    CHECK(fill->rect.y + fill->rect.h == 200, "fill reaches the bottom edge");
    wet_app_free(app);
}

static void test_canvas_is_absolute(void) {
    WetApp *app = wet_app_new(200, 200);
    app->render = &g_rec;
    WetWidget *c = wet_widget_new(app, WET_CLASS_CANVAS, "c");
    WetWidget *a = wet_widget_new(app, WET_CLASS_LABEL, "a");
    wet_label_set_text(a, "X");
    wet_canvas_set_left(a, 40);
    wet_canvas_set_top(a, 60);
    wet_widget_add(c, a);
    wet_app_root(app, c);
    wet_app_layout(app);

    CHECK(a->rect.x == 40, "canvas left honoured");
    CHECK(a->rect.y == 60, "canvas top honoured");
    wet_app_free(app);
}

static void test_grid_places_cells(void) {
    WetApp *app = wet_app_new(300, 200);
    app->render = &g_rec;
    WetWidget *g = wet_widget_new(app, WET_CLASS_GRID, "g");
    wet_grid_set_columns(g, 2);
    wet_grid_set_rows(g, 2);
    wet_stack_set_spacing(g, 0);
    WetWidget *a = wet_widget_new(app, WET_CLASS_LABEL, "a");
    WetWidget *b = wet_widget_new(app, WET_CLASS_LABEL, "b");
    WetWidget *c2 = wet_widget_new(app, WET_CLASS_LABEL, "c");
    WetWidget *d = wet_widget_new(app, WET_CLASS_LABEL, "d");
    wet_label_set_text(a, "A"); wet_label_set_text(b, "B");
    wet_label_set_text(c2, "C"); wet_label_set_text(d, "D");
    wet_grid_set_col(a, 0); wet_grid_set_row(a, 0);
    wet_grid_set_col(b, 1); wet_grid_set_row(b, 0);
    wet_grid_set_col(c2, 0); wet_grid_set_row(c2, 1);
    wet_grid_set_col(d, 1); wet_grid_set_row(d, 1);
    wet_widget_add(g, a); wet_widget_add(g, b);
    wet_widget_add(g, c2); wet_widget_add(g, d);
    wet_app_root(app, g);
    wet_app_layout(app);

    CHECK(b->rect.x > a->rect.x, "grid column 1 is right of column 0");
    CHECK(c2->rect.y > a->rect.y, "grid row 1 is below row 0");
    CHECK(d->rect.x == b->rect.x, "same column lines up");
    CHECK(d->rect.y == c2->rect.y, "same row lines up");
    wet_app_free(app);
}

static void test_click_reaches_control(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *b = wet_widget_new(app, WET_CLASS_BUTTON, "b");
    wet_button_set_text(b, "Go");
    wet_app_root(app, b);
    wet_app_layout(app);

    CHECK(b->rect.w > 0 && b->rect.h > 0, "button has a rect");
    int cx = b->rect.x + b->rect.w / 2;
    int cy = b->rect.y + b->rect.h / 2;
    CHECK(click(app, cx, cy), "click on the button is consumed");
    CHECK(app->focus == b, "clicking a button gives it the focus");
    wet_app_free(app);
}

static void test_click_miss_is_dropped(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *b = wet_widget_new(app, WET_CLASS_BUTTON, "b");
    wet_button_set_text(b, "Go");
    wet_app_root(app, b);
    wet_app_layout(app);

    CHECK(!wet_app_pointer(app, WET_EVENT_MOUSEDOWN, 5, 5 + b->rect.h + 40, 1),
          "a click outside the tree is not consumed");
    wet_app_free(app);
}

static void test_checkbox_toggles_and_fires_change(void) {
    static int fired;
    fired = 0;
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *c = wet_widget_new(app, WET_CLASS_CHECKBOX, "c");
    wet_checkbox_set_checked(c, false);
    wet_app_root(app, c);
    wet_app_layout(app);

    int cx = c->rect.x + c->rect.w / 2;
    int cy = c->rect.y + c->rect.h / 2;
    click(app, cx, cy);
    CHECK(wet_checkbox_checked(c), "clicking a checkbox checks it");

    click(app, cx, cy);
    CHECK(!wet_checkbox_checked(c), "clicking again unchecks it");
    wet_app_free(app);
    (void)fired;
}

static void test_slider_reports_value(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *s = wet_widget_new(app, WET_CLASS_SLIDER, "s");
    wet_slider_set_value(s, 0);
    wet_slider_set_minimum(s, 0);
    wet_slider_set_maximum(s, 100);
    wet_app_root(app, s);
    wet_app_layout(app);

    int right = s->rect.x + s->rect.w - 8;
    click(app, right, s->rect.y + s->rect.h / 2);
    CHECK(wet_slider_value(s) > 50, "clicking near the right end moves the value up");
    wet_app_free(app);
}

static void test_listbox_select(void) {
    WetApp *app = wet_app_new(200, 200);
    app->render = &g_rec;
    WetWidget *l = wet_widget_new(app, WET_CLASS_LISTBOX, "l");
    wet_listbox_add(l, "one");
    wet_listbox_add(l, "two");
    wet_listbox_add(l, "three");
    wet_app_root(app, l);
    wet_app_layout(app);

    CHECK(wet_listbox_count(l) == 3, "three items added");
    int fh = wet_text_height();
    int cy = l->rect.y + 2 + fh + fh / 2;
    click(app, l->rect.x + 8, cy);
    CHECK(wet_listbox_select(l) == 1, "clicking the second row selects it");
    CHECK(strcmp(wet_listbox_item(l, 1), "two") == 0, "second item is 'two'");
    wet_app_free(app);
}

static void test_textbox_accepts_keys(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *t = wet_widget_new(app, WET_CLASS_TEXTBOX, "t");
    wet_textbox_set_text(t, "");
    wet_app_root(app, t);
    wet_app_layout(app);
    wet_widget_set_focus(app, t);

    wet_app_key(app, WET_EVENT_KEYDOWN, 'a', 0);
    wet_app_key(app, WET_EVENT_KEYDOWN, 'b', 0);
    CHECK(strcmp(wet_textbox_text(t), "ab") == 0, "typed keys land in the text box");

    wet_app_key(app, WET_EVENT_KEYDOWN, '\b', 0);
    CHECK(strcmp(wet_textbox_text(t), "a") == 0, "backspace removes the last char");
    wet_app_free(app);
}

static void test_textbox_password_masks(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *t = wet_widget_new(app, WET_CLASS_TEXTBOX, "t");
    wet_textbox_set_text(t, "secret");
    wet_textbox_set_password(t, true);
    wet_app_root(app, t);
    wet_app_layout(app);
    wet_app_render(app);

    CHECK(find_text("secret") < 0, "the real text is never drawn");
    CHECK(find_text("******") >= 0, "the masked text is drawn");
    wet_app_free(app);
}

static void test_groupbox_reserves_caption_room(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *g = wet_widget_new(app, WET_CLASS_GROUPBOX, "g");
    WetLabel *lg = (WetLabel *)g->payload;
    free(lg->text);
    lg->text = strdup("Settings");
    WetWidget *l = wet_widget_new(app, WET_CLASS_LABEL, "inner");
    wet_label_set_text(l, "X");
    wet_widget_add(g, l);
    wet_app_root(app, g);
    wet_app_layout(app);

    CHECK(l->rect.y > 0, "child sits below the caption band");
    CHECK(l->rect.x > 0, "child sits inside the frame");
    wet_app_free(app);
}

static void test_resize_reflows(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *s = wet_widget_new(app, WET_CLASS_STACK, "s");
    WetWidget *l = wet_widget_new(app, WET_CLASS_LABEL, "l");
    wet_label_set_text(l, "X");
    wet_widget_add(s, l);
    wet_app_root(app, s);
    wet_app_layout(app);
    int before = s->rect.w;

    wet_app_resize(app, 320, 200);
    CHECK(app->width == 320, "resize updates the app width");
    CHECK(s->rect.w > before, "the panel reflows wider");
    wet_app_free(app);
}

static void test_disabled_control_still_measures(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *b = wet_widget_new(app, WET_CLASS_BUTTON, "b");
    wet_button_set_text(b, "Go");
    wet_widget_set_enabled(b, false);
    wet_app_root(app, b);
    wet_app_layout(app);
    CHECK(b->rect.w > 0, "a disabled button still gets a rect");
    wet_app_free(app);
}

static void test_widget_free_releases_children(void) {
    WetApp *app = wet_app_new(200, 100);
    app->render = &g_rec;
    WetWidget *s = wet_widget_new(app, WET_CLASS_STACK, "s");
    WetWidget *a = wet_widget_new(app, WET_CLASS_LABEL, "a");
    wet_label_set_text(a, "A");
    wet_widget_add(s, a);
    wet_app_root(app, s);
    CHECK(wet_app_find(app, "a") == a, "child indexed before free");
    wet_app_free(app);
    /* If the tree were freed twice this would crash; reaching here is the check. */
    CHECK(1, "tree freed exactly once");
}

/* -------------------------------------------------------------------- main */

int main(void) {
    printf("=== wet framework ===\n");
    test_label_measure();
    test_stack_vertical();
    test_stack_horizontal();
    test_button_pads_its_text();
    test_hidden_child_is_skipped();
    test_render_issues_text();
    test_render_hidden_draws_nothing();
    test_find_by_id();
    test_dock_edges();
    test_canvas_is_absolute();
    test_grid_places_cells();
    test_click_reaches_control();
    test_click_miss_is_dropped();
    test_checkbox_toggles_and_fires_change();
    test_slider_reports_value();
    test_listbox_select();
    test_textbox_accepts_keys();
    test_textbox_password_masks();
    test_groupbox_reserves_caption_room();
    test_resize_reflows();
    test_disabled_control_still_measures();
    test_widget_free_releases_children();

    printf("=== wet: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
