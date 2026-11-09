/*
 * wet_controls.c -- the leaf controls.
 *
 * Each one is a class: init, measure, arrange, render, input, destroy.  A
 * control measures itself from its own text (never from its current rect), so
 * the layout pass is honest -- a Stack can size a Button from the label it
 * will carry, not the space it happens to occupy.
 *
 * Controls never position themselves.  They only draw inside the rect a panel
 * gave them.
 */
#include "wet_internal.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ Label */

static void label_init(WetWidget *w) {
    WetLabel *l = (WetLabel *)w->payload;
    l->text  = strdup("");
    l->color = wet_theme_get()->text;
    l->wrap  = false;
}

static void label_destroy(WetWidget *w) {
    free(((WetLabel *)w->payload)->text);
}

static void label_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    WetLabel *l = (WetLabel *)w->payload;
    int fh = wet_text_height();
    if (l->wrap && avail.w > 0) {
        int cell = 6 * wet_theme_get()->font_scale;
        int per  = avail.w / (cell ? cell : 1);
        if (per < 1) per = 1;
        int len  = (int)strlen(l->text ? l->text : "");
        int rows = (len + per - 1) / per;
        if (rows < 1) rows = 1;
        *desired = (WetSize){ avail.w, rows * fh };
    } else {
        *desired = (WetSize){ wet_text_width(l->text), fh };
    }
}

static void label_arrange(WetWidget *w, WetRect r) { (void)w; (void)r; }

static void label_render(WetWidget *w, WetRender *r) {
    WetLabel *l = (WetLabel *)w->payload;
    const WetTheme *t = wet_theme_get();
    WetColor c = w->enabled ? l->color : t->text_dim;
    wet_r_text(r, w->rect, w->rect.x, w->rect.y, l->text ? l->text : "", c);
}

/* ----------------------------------------------------------------- Button */

static void button_init(WetWidget *w) {
    WetButton *b = (WetButton *)w->payload;
    b->text  = strdup("");
    b->color = wet_theme_get()->text;
}

static void button_destroy(WetWidget *w) { free(((WetButton *)w->payload)->text); }

static void button_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    (void)avail;
    WetButton *b = (WetButton *)w->payload;
    const WetTheme *t = wet_theme_get();
    int tw = wet_text_width(b->text);
    int th = wet_text_height();
    int pad = 4 + (b->default_button ? 4 : 0);
    *desired = (WetSize){ tw + pad * 2 + t->font_scale * 2, th + pad * 2 };
}

static void button_arrange(WetWidget *w, WetRect r) { (void)w; (void)r; }

static void button_render(WetWidget *w, WetRender *r) {
    WetButton *b = (WetButton *)w->payload;
    const WetTheme *t = wet_theme_get();
    bool hot = w->focused;
    WetColor face = w->enabled ? t->surface : t->window;
    wet_r_raised(r, w->rect, w->rect, face);
    if (b->default_button)
        wet_r_rect(r, w->rect, (WetRect){ w->rect.x + 2, w->rect.y + 2,
                                          w->rect.w - 4, w->rect.h - 4 }, t->border);
    if (hot) wet_r_rect(r, w->rect, (WetRect){ w->rect.x + 3, w->rect.y + 3,
                                              w->rect.w - 6, w->rect.h - 6 }, t->accent);
    WetColor c = w->enabled ? (hot ? t->accent_text : b->color) : t->text_dim;
    int tw = wet_text_width(b->text);
    int th = wet_text_height();
    int tx = w->rect.x + (w->rect.w - tw) / 2;
    int ty = w->rect.y + (w->rect.h - th) / 2;
    wet_r_text(r, w->rect, tx, ty, b->text ? b->text : "", c);
}

static int button_input(WetWidget *w, const void *ev) {
    const WetEvent *e = (const WetEvent *)ev;
    if (e->kind == WET_EVENT_CLICK || e->kind == WET_EVENT_KEYDOWN) {
        WetEvent out; memset(&out, 0, sizeof out);
        out.kind = WET_EVENT_CLICK; out.widget = w; out.target = w;
        wet_dispatch(w->app, w, &out);
        return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- TextBox */

static void textbox_init(WetWidget *w) {
    WetTextBox *t = (WetTextBox *)w->payload;
    t->text = strdup("");
}

static void textbox_destroy(WetWidget *w) { free(((WetTextBox *)w->payload)->text); }

static void textbox_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    WetTextBox *t = (WetTextBox *)w->payload;
    const WetTheme *t2 = wet_theme_get();
    int fh = wet_text_height();
    int rows = t->multiline ? 4 : 1;
    int inner = t->text ? wet_text_width(t->text) : 0;
    int want = inner + fh;
    if (avail.w > 0 && want > avail.w) want = avail.w;
    if (want < fh * 2) want = fh * 2;
    *desired = (WetSize){ want, rows * fh + t2->font_scale * 2 };
}

static void textbox_arrange(WetWidget *w, WetRect r) { (void)w; (void)r; }

static void textbox_render(WetWidget *w, WetRender *r) {
    WetTextBox *t = (WetTextBox *)w->payload;
    const WetTheme *th = wet_theme_get();
    wet_r_sunken(r, w->rect, w->rect, th->sunken);
    if (w->focused)
        wet_r_rect(r, w->rect, (WetRect){ w->rect.x + 2, w->rect.y + 2,
                                          w->rect.w - 4, w->rect.h - 4 },
                   th->accent);
    const char *s = t->text ? t->text : "";
    char masked[1024];
    if (t->password) {
        size_t n = strlen(s);
        if (n >= sizeof masked) n = sizeof masked - 1;
        memset(masked, '*', n);
        masked[n] = 0;
        s = masked;
    }
    WetColor c = w->enabled ? th->text : th->text_dim;
    wet_r_text(r, w->rect, w->rect.x + 4, w->rect.y + 3, s, c);
}

static int textbox_input(WetWidget *w, const void *ev) {
    const WetEvent *e = (const WetEvent *)ev;
    WetTextBox *t = (WetTextBox *)w->payload;
    if (e->kind == WET_EVENT_KEYDOWN) {
        if (e->key == '\b') {
            size_t n = strlen(t->text ? t->text : "");
            if (n) {
                char *nt = (char *)malloc(n);
                if (nt) { memcpy(nt, t->text, n - 1); nt[n - 1] = 0;
                           free(t->text); t->text = nt; }
            }
        } else if (e->key >= 32 && e->key < 127) {
            size_t n = strlen(t->text ? t->text : "");
            char *nt = (char *)malloc(n + 2);
            if (nt) { if (t->text) memcpy(nt, t->text, n);
                      nt[n] = (char)e->key; nt[n + 1] = 0;
                      free(t->text); t->text = nt; }
        } else {
            return 0;
        }
        WetEvent out; memset(&out, 0, sizeof out);
        out.kind = WET_EVENT_TEXTCHANGED; out.widget = w; out.target = w;
        wet_dispatch(w->app, w, &out);
        return 1;
    }
    return 0;
}

/* --------------------------------------------------------------- CheckBox */

static void checkbox_init(WetWidget *w) {
    WetCheckBox *c = (WetCheckBox *)w->payload;
    c->text = strdup("");
}

static void checkbox_destroy(WetWidget *w) { free(((WetCheckBox *)w->payload)->text); }

static void checkbox_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    (void)avail;
    WetCheckBox *c = (WetCheckBox *)w->payload;
    int box = wet_text_height();
    int tw  = wet_text_width(c->text);
    *desired = (WetSize){ box + 4 + tw, box };
}

static void checkbox_arrange(WetWidget *w, WetRect r) { (void)w; (void)r; }

static void checkbox_render(WetWidget *w, WetRender *r) {
    WetCheckBox *c = (WetCheckBox *)w->payload;
    const WetTheme *t = wet_theme_get();
    int fh = wet_text_height();
    int box = fh;
    WetRect b = (WetRect){ w->rect.x, w->rect.y + (w->rect.h - box) / 2, box, box };
    wet_r_sunken(r, w->rect, b, t->sunken);
    if (c->checked) {
        WetColor m = w->enabled ? t->text : t->text_dim;
        wet_r_rect(r, w->rect, (WetRect){ b.x + 2, b.y + 2, b.w - 4, 1 }, m);
        wet_r_rect(r, w->rect, (WetRect){ b.x + 2, b.y + 2, 1, b.h - 4 }, m);
        wet_r_rect(r, w->rect, (WetRect){ b.x + 2, b.y + b.h / 2 - 1, b.w - 4, 2 }, m);
        wet_r_rect(r, w->rect, (WetRect){ b.x + b.w / 2 - 1, b.y + 2, 2, b.h - 4 }, m);
    }
    WetColor col = w->enabled ? t->text : t->text_dim;
    wet_r_text(r, w->rect, b.x + box + 4, w->rect.y + (w->rect.h - fh) / 2,
               c->text ? c->text : "", col);
}

static int checkbox_input(WetWidget *w, const void *ev) {
    const WetEvent *e = (const WetEvent *)ev;
    if (e->kind != WET_EVENT_CLICK) return 0;
    WetCheckBox *c = (WetCheckBox *)w->payload;
    c->checked = !c->checked;
    WetEvent out; memset(&out, 0, sizeof out);
    out.kind = WET_EVENT_CHANGE; out.widget = w; out.target = w;
    out.value = c->checked;
    wet_dispatch(w->app, w, &out);
    return 1;
}

/* ---------------------------------------------------------------- ListBox */

static void listbox_init(WetWidget *w) {
    WetListBox *l = (WetListBox *)w->payload;
    l->selected = -1;
}

static void listbox_destroy(WetWidget *w) {
    WetListBox *l = (WetListBox *)w->payload;
    for (int i = 0; i < l->nitems; i++) free(l->items[i]);
    free(l->items);
}

static void listbox_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    WetListBox *l = (WetListBox *)w->payload;
    const WetTheme *t = wet_theme_get();
    int fh = wet_text_height();
    int rows = l->nitems > 0 ? l->nitems : 4;
    int widest = 0;
    for (int i = 0; i < l->nitems; i++) {
        int iw = wet_text_width(l->items[i]);
        if (iw > widest) widest = iw;
    }
    int want = widest + fh * 2;
    if (avail.w > 0 && want > avail.w) want = avail.w;
    *desired = (WetSize){ want, rows * fh + t->font_scale * 2 };
}

static void listbox_arrange(WetWidget *w, WetRect r) { (void)w; (void)r; }

static void listbox_render(WetWidget *w, WetRender *r) {
    WetListBox *l = (WetListBox *)w->payload;
    const WetTheme *t = wet_theme_get();
    wet_r_sunken(r, w->rect, w->rect, t->sunken);
    int fh = wet_text_height();
    WetRect inner = (WetRect){ w->rect.x + 2, w->rect.y + 2,
                                w->rect.w - 4, w->rect.h - 4 };
    for (int i = 0; i < l->nitems; i++) {
        int y = inner.y + i * fh - l->scroll;
        if (y + fh < inner.y || y > inner.y + inner.h) continue;
        if (i == l->selected) {
            wet_r_fill(r, w->rect, (WetRect){ inner.x, y, inner.w, fh }, t->selection);
            wet_r_text(r, w->rect, inner.x + 2, y, l->items[i], t->selection_text);
        } else {
            wet_r_text(r, w->rect, inner.x + 2, y, l->items[i], t->text);
        }
    }
}

static int listbox_input(WetWidget *w, const void *ev) {
    const WetEvent *e = (const WetEvent *)ev;
    if (e->kind != WET_EVENT_CLICK) return 0;
    WetListBox *l = (WetListBox *)w->payload;
    int fh = wet_text_height();
    int inner_y = w->rect.y + 2;
    int idx = (e->y - inner_y) / fh + l->scroll;
    if (idx < 0 || idx >= l->nitems) return 0;
    if (l->selected == idx) return 0;
    l->selected = idx;
    WetEvent out; memset(&out, 0, sizeof out);
    out.kind = WET_EVENT_SELECT; out.widget = w; out.target = w;
    out.value = idx;
    wet_dispatch(w->app, w, &out);
    return 1;
}

/* -------------------------------------------------------------- Progress */

static void progress_init(WetWidget *w) {
    WetProgress *p = (WetProgress *)w->payload;
    p->maximum = 100;
    p->minimum = 0;
    p->value   = 0;
}

static void progress_destroy(WetWidget *w) { (void)w; }

static void progress_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    (void)w; (void)avail;
    *desired = (WetSize){ 120, wet_text_height() + 2 };
}

static void progress_arrange(WetWidget *w, WetRect r) { (void)w; (void)r; }

static void progress_render(WetWidget *w, WetRender *r) {
    WetProgress *p = (WetProgress *)w->payload;
    const WetTheme *t = wet_theme_get();
    wet_r_sunken(r, w->rect, w->rect, t->sunken);
    int range = p->maximum - p->minimum;
    if (range <= 0) range = 1;
    int v = p->value - p->minimum;
    if (v < 0) v = 0;
    if (v > range) v = range;
    int fill_w = (w->rect.w - 4) * v / range;
    if (fill_w > 0)
        wet_r_fill(r, w->rect, (WetRect){ w->rect.x + 2, w->rect.y + 2,
                                          fill_w, w->rect.h - 4 },
                   w->enabled ? t->accent : t->border);
}

/* ----------------------------------------------------------------- Slider */

static void slider_init(WetWidget *w) {
    WetSlider *s = (WetSlider *)w->payload;
    s->minimum = 0; s->maximum = 100; s->value = 0; s->step = 1;
}

static void slider_destroy(WetWidget *w) { (void)w; }

static void slider_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    (void)w; (void)avail;
    *desired = (WetSize){ 120, wet_text_height() + 2 };
}

static void slider_arrange(WetWidget *w, WetRect r) { (void)w; (void)r; }

static void slider_render(WetWidget *w, WetRender *r) {
    WetSlider *s = (WetSlider *)w->payload;
    const WetTheme *t = wet_theme_get();
    int mid = w->rect.y + w->rect.h / 2;
    wet_r_rect(r, w->rect, (WetRect){ w->rect.x + 4, mid, w->rect.w - 8, 1 }, t->border_shade);
    int range = s->maximum - s->minimum;
    if (range <= 0) range = 1;
    int frac = (s->value - s->minimum) * (w->rect.w - 20) / range;
    int tx = w->rect.x + 8 + frac;
    WetRect thumb = (WetRect){ tx - 4, mid - 6, 8, 12 };
    wet_r_raised(r, w->rect, thumb, w->enabled ? t->surface : t->window);
}

static int slider_input(WetWidget *w, const void *ev) {
    const WetEvent *e = (const WetEvent *)ev;
    if (e->kind != WET_EVENT_CLICK) return 0;
    WetSlider *s = (WetSlider *)w->payload;
    int range = s->maximum - s->minimum;
    if (range <= 0) return 0;
    int frac = (e->x - w->rect.x - 8) * range / (w->rect.w - 20);
    int v = s->minimum + frac;
    if (v < s->minimum) v = s->minimum;
    if (v > s->maximum) v = s->maximum;
    if (v == s->value) return 0;
    s->value = v;
    WetEvent out; memset(&out, 0, sizeof out);
    out.kind = WET_EVENT_CHANGE; out.widget = w; out.target = w;
    out.value = v;
    wet_dispatch(w->app, w, &out);
    return 1;
}

/* ------------------------------------------------------------- Separator */

static void separator_init(WetWidget *w) { ((WetSeparator *)w->payload)->vertical = false; }
static void separator_destroy(WetWidget *w) { (void)w; }

static void separator_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    (void)avail;
    bool vert = ((WetSeparator *)w->payload)->vertical;
    *desired = vert ? (WetSize){ 2, 0 } : (WetSize){ 0, 2 };
}

static void separator_arrange(WetWidget *w, WetRect r) { (void)w; (void)r; }

static void separator_render(WetWidget *w, WetRender *r) {
    const WetTheme *t = wet_theme_get();
    if (((WetSeparator *)w->payload)->vertical) {
        wet_r_rect(r, w->rect, (WetRect){ w->rect.x, w->rect.y, 1, w->rect.h }, t->border_shade);
        wet_r_rect(r, w->rect, (WetRect){ w->rect.x + 1, w->rect.y, 1, w->rect.h }, t->border_lit);
    } else {
        wet_r_rect(r, w->rect, (WetRect){ w->rect.x, w->rect.y, w->rect.w, 1 }, t->border_shade);
        wet_r_rect(r, w->rect, (WetRect){ w->rect.x, w->rect.y + 1, w->rect.w, 1 }, t->border_lit);
    }
}

/* --------------------------------------------------------------- GroupBox */

static void groupbox_init(WetWidget *w) { ((WetGroupBox *)w->payload)->text = strdup(""); }
static void groupbox_destroy(WetWidget *w) { free(((WetGroupBox *)w->payload)->text); }

static void groupbox_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    WetGroupBox *g = (WetGroupBox *)w->payload;
    int fh = wet_text_height();
    int tw = wet_text_width(g->text);
    if (w->nchildren) {
        WetSize child; child.w = child.h = 0;
        for (int i = 0; i < w->nchildren; i++) {
            WetWidget *c = w->children[i];
            wet_widget_measure(c, avail);
            if (c->desired.w > child.w) child.w = c->desired.w;
            if (c->desired.h > child.h) child.h = c->desired.h;
        }
        *desired = (WetSize){ child.w + 12, child.h + fh + 12 };
    } else {
        *desired = (WetSize){ tw + 12, fh + 12 };
    }
}

static void groupbox_arrange(WetWidget *w, WetRect r) {
    int fh = wet_text_height();
    WetRect inner = (WetRect){ r.x + 6, r.y + fh + 4, r.w - 12, r.h - fh - 10 };
    if (inner.w < 0) inner.w = 0;
    if (inner.h < 0) inner.h = 0;
    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (c->visible) wet_widget_arrange(c, inner);
    }
}

static void groupbox_render(WetWidget *w, WetRender *r) {
    WetGroupBox *g = (WetGroupBox *)w->payload;
    const WetTheme *t = wet_theme_get();
    int fh = wet_text_height();
    int tw = wet_text_width(g->text);
    /* Frame, with a gap behind the caption. */
    wet_r_rect(r, w->rect, (WetRect){ w->rect.x, w->rect.y + fh / 2,
                                      w->rect.w, w->rect.h - fh / 2 }, t->border);
    wet_r_fill(r, w->rect, (WetRect){ w->rect.x + 4, w->rect.y, tw + 6, fh }, t->surface);
    wet_r_text(r, w->rect, w->rect.x + 6, w->rect.y, g->text ? g->text : "", t->text);
    for (int i = 0; i < w->nchildren; i++) {
        WetWidget *c = w->children[i];
        if (c->visible) wet_widget_render(c, r);
    }
}

/* ------------------------------------------------------------------ Image */

static void image_init(WetWidget *w) {
    WetImage *im = (WetImage *)w->payload;
    im->width = im->height = 0;
    im->pixels = NULL;
}

static void image_destroy(WetWidget *w) {
    WetImage *im = (WetImage *)w->payload;
    free(im->pixels);
    free(im->path);
}

static void image_measure(WetWidget *w, WetSize avail, WetSize *desired) {
    WetImage *im = (WetImage *)w->payload;
    (void)avail;
    *desired = (WetSize){ im->width ? im->width : 32, im->height ? im->height : 32 };
}

static void image_arrange(WetWidget *w, WetRect r) { (void)w; (void)r; }

static void image_render(WetWidget *w, WetRender *r) {
    WetImage *im = (WetImage *)w->payload;
    const WetTheme *t = wet_theme_get();
    wet_r_sunken(r, w->rect, w->rect, t->surface);
    if (im->pixels && im->width > 0 && im->height > 0) {
        for (int y = 0; y < im->height && y < w->rect.h; y++) {
            for (int x = 0; x < im->width && x < w->rect.w; x++) {
                wet_r_fill(r, w->rect,
                           (WetRect){ w->rect.x + x, w->rect.y + y, 1, 1 },
                           im->pixels[y * im->width + x]);
            }
        }
    }
}

/* ------------------------------------------------------------ registration */

void wet_controls_register(void) {
    static const WetWidgetClass label_cls = {
        "label", WET_CLASS_LABEL, sizeof(WetLabel),
        label_init, label_measure, label_arrange, label_render, NULL, label_destroy
    };
    static const WetWidgetClass button_cls = {
        "button", WET_CLASS_BUTTON, sizeof(WetButton),
        button_init, button_measure, button_arrange, button_render, button_input, button_destroy
    };
    static const WetWidgetClass textbox_cls = {
        "textbox", WET_CLASS_TEXTBOX, sizeof(WetTextBox),
        textbox_init, textbox_measure, textbox_arrange, textbox_render, textbox_input, textbox_destroy
    };
    static const WetWidgetClass checkbox_cls = {
        "checkbox", WET_CLASS_CHECKBOX, sizeof(WetCheckBox),
        checkbox_init, checkbox_measure, checkbox_arrange, checkbox_render, checkbox_input, checkbox_destroy
    };
    static const WetWidgetClass listbox_cls = {
        "listbox", WET_CLASS_LISTBOX, sizeof(WetListBox),
        listbox_init, listbox_measure, listbox_arrange, listbox_render, listbox_input, listbox_destroy
    };
    static const WetWidgetClass progress_cls = {
        "progress", WET_CLASS_PROGRESS, sizeof(WetProgress),
        progress_init, progress_measure, progress_arrange, progress_render, NULL, progress_destroy
    };
    static const WetWidgetClass slider_cls = {
        "slider", WET_CLASS_SLIDER, sizeof(WetSlider),
        slider_init, slider_measure, slider_arrange, slider_render, slider_input, slider_destroy
    };
    static const WetWidgetClass separator_cls = {
        "separator", WET_CLASS_SEPARATOR, sizeof(WetSeparator),
        separator_init, separator_measure, separator_arrange, separator_render, NULL, separator_destroy
    };
    static const WetWidgetClass groupbox_cls = {
        "groupbox", WET_CLASS_GROUPBOX, sizeof(WetGroupBox),
        groupbox_init, groupbox_measure, groupbox_arrange, groupbox_render, NULL, groupbox_destroy
    };
    static const WetWidgetClass image_cls = {
        "image", WET_CLASS_IMAGE, sizeof(WetImage),
        image_init, image_measure, image_arrange, image_render, NULL, image_destroy
    };
    wet_class_register(&label_cls);
    wet_class_register(&button_cls);
    wet_class_register(&textbox_cls);
    wet_class_register(&checkbox_cls);
    wet_class_register(&listbox_cls);
    wet_class_register(&progress_cls);
    wet_class_register(&slider_cls);
    wet_class_register(&separator_cls);
    wet_class_register(&groupbox_cls);
    wet_class_register(&image_cls);
}

/* ------------------------------------------------------------- accessors */

const char *wet_label_text(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_LABEL) ? ((WetLabel *)w->payload)->text : NULL;
}
void wet_label_set_text(WetWidget *w, const char *text) {
    if (!w || w->cls->id != WET_CLASS_LABEL) return;
    WetLabel *l = (WetLabel *)w->payload;
    free(l->text);
    l->text = strdup(text ? text : "");
}

const char *wet_button_text(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_BUTTON) ? ((WetButton *)w->payload)->text : NULL;
}
void wet_button_set_text(WetWidget *w, const char *text) {
    if (!w || w->cls->id != WET_CLASS_BUTTON) return;
    WetButton *b = (WetButton *)w->payload;
    free(b->text);
    b->text = strdup(text ? text : "");
}

const char *wet_textbox_text(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_TEXTBOX) ? ((WetTextBox *)w->payload)->text : NULL;
}
void wet_textbox_set_text(WetWidget *w, const char *text) {
    if (!w || w->cls->id != WET_CLASS_TEXTBOX) return;
    WetTextBox *t = (WetTextBox *)w->payload;
    free(t->text);
    t->text = strdup(text ? text : "");
}
bool wet_textbox_password(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_TEXTBOX) ? ((WetTextBox *)w->payload)->password : false;
}
void wet_textbox_set_password(WetWidget *w, bool on) {
    if (w && w->cls->id == WET_CLASS_TEXTBOX) ((WetTextBox *)w->payload)->password = on;
}

bool wet_checkbox_checked(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_CHECKBOX) ? ((WetCheckBox *)w->payload)->checked : false;
}
void wet_checkbox_set_checked(WetWidget *w, bool on) {
    if (w && w->cls->id == WET_CLASS_CHECKBOX) ((WetCheckBox *)w->payload)->checked = on;
}

int         wet_listbox_count  (const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_LISTBOX) ? ((WetListBox *)w->payload)->nitems : 0;
}
const char *wet_listbox_item   (const WetWidget *w, int index) {
    if (!w || w->cls->id != WET_CLASS_LISTBOX) return NULL;
    WetListBox *l = (WetListBox *)w->payload;
    if (index < 0 || index >= l->nitems) return NULL;
    return l->items[index];
}
int  wet_listbox_select(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_LISTBOX) ? ((WetListBox *)w->payload)->selected : -1;
}
void wet_listbox_add(WetWidget *w, const char *text) {
    if (!w || w->cls->id != WET_CLASS_LISTBOX) return;
    WetListBox *l = (WetListBox *)w->payload;
    if (l->nitems == l->cap) {
        int cap = l->cap ? l->cap * 2 : 8;
        char **grown = (char **)realloc(l->items, (size_t)cap * sizeof *grown);
        if (!grown) return;
        l->items = grown;
        l->cap = cap;
    }
    l->items[l->nitems++] = strdup(text ? text : "");
}
void wet_listbox_set_select(WetWidget *w, int index) {
    if (!w || w->cls->id != WET_CLASS_LISTBOX) return;
    WetListBox *l = (WetListBox *)w->payload;
    if (index < -1 || index >= l->nitems) return;
    l->selected = index;
}

int wet_progress_value(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_PROGRESS) ? ((WetProgress *)w->payload)->value : 0;
}
void wet_progress_set_value(WetWidget *w, int v) {
    if (w && w->cls->id == WET_CLASS_PROGRESS) {
        WetProgress *p = (WetProgress *)w->payload;
        if (v < p->minimum) v = p->minimum;
        if (v > p->maximum) v = p->maximum;
        p->value = v;
    }
}
int wet_progress_maximum(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_PROGRESS) ? ((WetProgress *)w->payload)->maximum : 100;
}
void wet_progress_set_maximum(WetWidget *w, int v) {
    if (w && w->cls->id == WET_CLASS_PROGRESS) ((WetProgress *)w->payload)->maximum = v;
}

int wet_slider_value(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_SLIDER) ? ((WetSlider *)w->payload)->value : 0;
}
void wet_slider_set_value(WetWidget *w, int v) {
    if (w && w->cls->id == WET_CLASS_SLIDER) {
        WetSlider *s = (WetSlider *)w->payload;
        if (v < s->minimum) v = s->minimum;
        if (v > s->maximum) v = s->maximum;
        s->value = v;
    }
}
int wet_slider_minimum(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_SLIDER) ? ((WetSlider *)w->payload)->minimum : 0;
}
int wet_slider_maximum(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_SLIDER) ? ((WetSlider *)w->payload)->maximum : 100;
}
void wet_slider_set_minimum(WetWidget *w, int v) {
    if (w && w->cls->id == WET_CLASS_SLIDER) ((WetSlider *)w->payload)->minimum = v;
}
void wet_slider_set_maximum(WetWidget *w, int v) {
    if (w && w->cls->id == WET_CLASS_SLIDER) ((WetSlider *)w->payload)->maximum = v;
}

int wet_image_width(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_IMAGE) ? ((WetImage *)w->payload)->width : 0;
}
int wet_image_height(const WetWidget *w) {
    return (w && w->cls->id == WET_CLASS_IMAGE) ? ((WetImage *)w->payload)->height : 0;
}
