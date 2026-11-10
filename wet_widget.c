/*
 * wet_widget.c -- the widget tree, the class registry, text measurement and
 * the theme palette.  Owns nothing else; panels live in wet_layout.c and
 * controls in wet_controls.c.
 */
#include "wet_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------------------------------------------------------------- theme */

const WetTheme wet_theme_default = {
    .name          = "light",
    .window        = WET_RGB(0xF0, 0xF0, 0xF0),
    .surface       = WET_RGB(0xFF, 0xFF, 0xFF),
    .sunken        = WET_RGB(0xFF, 0xFF, 0xFF),
    .border        = WET_RGB(0x80, 0x80, 0x80),
    .border_lit    = WET_RGB(0xDF, 0xDF, 0xDF),
    .border_shade  = WET_RGB(0x40, 0x40, 0x40),
    .text          = WET_RGB(0x00, 0x00, 0x00),
    .text_dim      = WET_RGB(0x60, 0x60, 0x60),
    .accent        = WET_RGB(0x00, 0x3C, 0x8C),
    .accent_text   = WET_RGB(0xFF, 0xFF, 0xFF),
    .selection     = WET_RGB(0x33, 0x99, 0xFF),
    .selection_text= WET_RGB(0xFF, 0xFF, 0xFF),
    .font_scale    = 1,
};

/* The 1998 shell: a teal desktop, silver chrome, navy accents. */
const WetTheme wet_theme_win98 = {
    .name          = "win98",
    .window        = WET_RGB(0x00, 0x80, 0x80),
    .surface       = WET_RGB(0xC0, 0xC0, 0xC0),
    .sunken        = WET_RGB(0xFF, 0xFF, 0xFF),
    .border        = WET_RGB(0x80, 0x80, 0x80),
    .border_lit    = WET_RGB(0xDF, 0xDF, 0xDF),
    .border_shade  = WET_RGB(0x40, 0x40, 0x40),
    .text          = WET_RGB(0x00, 0x00, 0x00),
    .text_dim      = WET_RGB(0x40, 0x40, 0x40),
    .accent        = WET_RGB(0x00, 0x00, 0x80),
    .accent_text   = WET_RGB(0xFF, 0xFF, 0xFF),
    .selection     = WET_RGB(0x00, 0x00, 0x80),
    .selection_text= WET_RGB(0xFF, 0xFF, 0xFF),
    .font_scale    = 1,
};

/* Luna: the XP blue over a warm stone surface. */
const WetTheme wet_theme_xp = {
    .name          = "xp",
    .window        = WET_RGB(0xEC, 0xE9, 0xD8),
    .surface       = WET_RGB(0xF1, 0xEF, 0xE2),
    .sunken        = WET_RGB(0xFF, 0xFF, 0xFF),
    .border        = WET_RGB(0x91, 0x9B, 0x9C),
    .border_lit    = WET_RGB(0xFF, 0xFF, 0xFF),
    .border_shade  = WET_RGB(0x71, 0x6F, 0x64),
    .text          = WET_RGB(0x00, 0x00, 0x00),
    .text_dim      = WET_RGB(0x6D, 0x6D, 0x6D),
    .accent        = WET_RGB(0x31, 0x6A, 0xC5),
    .accent_text   = WET_RGB(0xFF, 0xFF, 0xFF),
    .selection     = WET_RGB(0x31, 0x6A, 0xC5),
    .selection_text= WET_RGB(0xFF, 0xFF, 0xFF),
    .font_scale    = 1,
};

/* WuBu green on a deep field -- the house theme. */
const WetTheme wet_theme_wubu = {
    .name          = "wubu",
    .window        = WET_RGB(0x0E, 0x2A, 0x1E),
    .surface       = WET_RGB(0x14, 0x35, 0x2A),
    .sunken        = WET_RGB(0x0A, 0x1F, 0x16),
    .border        = WET_RGB(0x2E, 0x6B, 0x52),
    .border_lit    = WET_RGB(0x3E, 0x8A, 0x68),
    .border_shade  = WET_RGB(0x06, 0x12, 0x0D),
    .text          = WET_RGB(0xE8, 0xF5, 0xEE),
    .text_dim      = WET_RGB(0x8F, 0xB8, 0xA5),
    .accent        = WET_RGB(0x2F, 0xA3, 0x6B),
    .accent_text   = WET_RGB(0x04, 0x14, 0x0C),
    .selection     = WET_RGB(0x2F, 0xA3, 0x6B),
    .selection_text= WET_RGB(0x04, 0x14, 0x0C),
    .font_scale    = 1,
};

/* A neutral dark surface, for low light and for the GPU effect pass to sit on. */
const WetTheme wet_theme_dark = {
    .name          = "dark",
    .window        = WET_RGB(0x1E, 0x1E, 0x1E),
    .surface       = WET_RGB(0x2A, 0x2A, 0x2A),
    .sunken        = WET_RGB(0x16, 0x16, 0x16),
    .border        = WET_RGB(0x4A, 0x4A, 0x4A),
    .border_lit    = WET_RGB(0x5A, 0x5A, 0x5A),
    .border_shade  = WET_RGB(0x10, 0x10, 0x10),
    .text          = WET_RGB(0xE8, 0xE8, 0xE8),
    .text_dim      = WET_RGB(0x9A, 0x9A, 0x9A),
    .accent        = WET_RGB(0x4C, 0x8B, 0xF5),
    .accent_text   = WET_RGB(0xFF, 0xFF, 0xFF),
    .selection     = WET_RGB(0x4C, 0x8B, 0xF5),
    .selection_text= WET_RGB(0xFF, 0xFF, 0xFF),
    .font_scale    = 1,
};

static const WetTheme *const g_themes[] = {
    &wet_theme_default, &wet_theme_win98, &wet_theme_xp,
    &wet_theme_wubu,    &wet_theme_dark,
};

static const WetTheme *g_theme = &wet_theme_default;

const WetTheme *wet_theme_get(void) { return g_theme; }
void wet_theme_set(const WetTheme *t) { g_theme = t ? t : &wet_theme_default; }

int                wet_theme_count(void) { return (int)(sizeof g_themes / sizeof g_themes[0]); }

const WetTheme    *wet_theme_at(int i) {
    if (i < 0 || i >= wet_theme_count()) return NULL;
    return g_themes[i];
}

const char        *wet_theme_name_at(int i) {
    const WetTheme *t = wet_theme_at(i);
    return t ? t->name : NULL;
}

const WetTheme    *wet_theme_by_name(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < wet_theme_count(); i++)
        if (g_themes[i]->name && strcmp(g_themes[i]->name, name) == 0)
            return g_themes[i];
    return NULL;
}

const char *wet_theme_name(void) {
    return (g_theme && g_theme->name) ? g_theme->name : "custom";
}

/* ------------------------------------------------------------ text metrics
 * The VBE font is a fixed 8x8 cell; GLYPH_ADVANCE is the advance width.  A
 * control measures itself in whole cells so the numbers stay exact at any
 * text scale. */

#define GLYPH_W 8
#define GLYPH_H 8
#define GLYPH_ADVANCE 6

int wet_text_width(const char *s) {
    if (!s) return 0;
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        /* Count UTF-8 continuation bytes as part of the preceding cell. */
        if ((*p & 0xC0) != 0x80) n++;
    }
    return n * GLYPH_ADVANCE * g_theme->font_scale;
}

int wet_text_height(void) { return GLYPH_H * g_theme->font_scale; }

void wet_text_draw(WetRender *r, int x, int y, const char *s, WetColor c) {
    if (!r || !s) return;
    r->text(r->ctx, x, y, s, c);
}

/* ------------------------------------------------------------ class table */

#define WET_MAX_CLASSES 32
static const WetWidgetClass *g_classes[WET_MAX_CLASSES];
static int g_nclasses;

void wet_class_register(const WetWidgetClass *cls) {
    if (!cls || g_nclasses >= WET_MAX_CLASSES) return;
    g_classes[g_nclasses++] = cls;
}

const WetWidgetClass *wet_class_lookup(WetClassId id) {
    for (int i = 0; i < g_nclasses; i++)
        if (g_classes[i]->id == id) return g_classes[i];
    return NULL;
}

const WetWidgetClass *wet_class_by_name(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_nclasses; i++)
        if (strcmp(g_classes[i]->name, name) == 0) return g_classes[i];
    return NULL;
}

/* ------------------------------------------------------------------ tree */

static void wet_widget_free_children(WetWidget *w) {
    for (int i = 0; i < w->nchildren; i++) {
        wet_widget_free(w->children[i]);
        w->children[i] = NULL;
    }
    free(w->children);
    w->children = NULL;
    w->nchildren = w->capchildren = 0;
}

WetWidget *wet_widget_new(WetApp *app, WetClassId cls, const char *id) {
    const WetWidgetClass *c = wet_class_lookup(cls);
    if (!c) return NULL;

    WetWidget *w = (WetWidget *)calloc(1, sizeof *w);
    if (!w) return NULL;

    w->app     = app;
    w->cls     = c;
    w->visible = true;
    w->enabled = true;
    w->id      = id ? strdup(id) : NULL;
    w->slot.halign = -1;   /* -1 = "not set", so the panel decides */
    w->slot.valign = -1;

    if (c->payload_size) {
        w->payload = calloc(1, c->payload_size);
        if (!w->payload) { free(w->id); free(w); return NULL; }
    }
    if (c->init) c->init(w);

    if (app && w->id) wet_app_index(app, w);
    return w;
}

void wet_widget_free(WetWidget *w) {
    if (!w) return;
    wet_widget_free_children(w);
    if (w->cls->destroy) w->cls->destroy(w);
    if (w->app) {
        /* Unindex, so wet.find() cannot hand back a dead widget. */
        for (int i = 0; i < w->app->index_n; i++) {
            if (w->app->index[i] == w) {
                w->app->index[i] = w->app->index[--w->app->index_n];
                break;
            }
        }
    }
    free(w->id);
    free(w->payload);
    free(w);
}

WetWidget *wet_widget_parent(const WetWidget *w) { return w ? w->parent : NULL; }

WetWidget *wet_widget_child(const WetWidget *w, int index) {
    if (!w || index < 0 || index >= w->nchildren) return NULL;
    return w->children[index];
}

int wet_widget_nchildren(const WetWidget *w) { return w ? w->nchildren : 0; }

void wet_widget_add(WetWidget *parent, WetWidget *child) {
    if (!parent || !child || child == parent) return;
    if (child->parent) wet_widget_remove(child->parent, child);

    if (parent->nchildren == parent->capchildren) {
        int cap = parent->capchildren ? parent->capchildren * 2 : 4;
        WetWidget **grown = (WetWidget **)realloc(parent->children,
                                                  (size_t)cap * sizeof *grown);
        if (!grown) return;
        parent->children = grown;
        parent->capchildren = cap;
    }
    parent->children[parent->nchildren++] = child;
    child->parent = parent;
}

void wet_widget_remove(WetWidget *parent, WetWidget *child) {
    if (!parent || !child) return;
    for (int i = 0; i < parent->nchildren; i++) {
        if (parent->children[i] == child) {
            for (int j = i + 1; j < parent->nchildren; j++)
                parent->children[j - 1] = parent->children[j];
            parent->nchildren--;
            child->parent = NULL;
            return;
        }
    }
}

/* -------------------------------------------------------------- walk passes
 * The three entry points a panel or the app uses to run the layout and draw
 * passes over the whole tree.  A control calls these on its children; a
 * control never calls them on itself. */

void wet_widget_measure(WetWidget *w, WetSize avail) {
    if (!w) return;
    if (!w->visible) { w->desired = (WetSize){0,0}; return; }
    w->cls->measure(w, avail, &w->desired);
}

void wet_widget_arrange(WetWidget *w, WetRect r) {
    if (!w) return;
    w->rect = r;
    if (!w->visible) return;
    w->cls->arrange(w, r);
}

void wet_widget_render(WetWidget *w, WetRender *r) {
    if (!w || !w->visible || !r) return;
    const WetTheme *t = wet_theme_get();
    if (!w->enabled) {
        /* Draw disabled controls in place, but the caller may grey them. */
        (void)t;
    }
    w->cls->render(w, r);
}

/* -------------------------------------------------------------- accessors */

const char  *wet_widget_id     (const WetWidget *w) { return w ? w->id : NULL; }
WetClassId   wet_widget_class (const WetWidget *w) { return w ? w->cls->id : 0; }
const char  *wet_widget_class_name(const WetWidget *w) {
    return (w && w->cls) ? w->cls->name : NULL;
}
WetRect      wet_widget_rect  (const WetWidget *w) { return w ? w->rect : WET_RECT_EMPTY; }
WetSize      wet_widget_desired(const WetWidget *w) { return w ? w->desired : (WetSize){0,0}; }
void        *wet_widget_payload(WetWidget *w)      { return w ? w->payload : NULL; }

void  wet_widget_set_visible(WetWidget *w, bool v) { if (w) w->visible = v; }
bool  wet_widget_visible(const WetWidget *w)       { return w && w->visible; }
void  wet_widget_set_enabled(WetWidget *w, bool v) { if (w) w->enabled = v; }
bool  wet_widget_enabled(const WetWidget *w)       { return w && w->enabled; }

void wet_widget_set_focus(WetApp *app, WetWidget *w) {
    if (!app) return;
    if (app->focus == w) return;
    WetEvent ev; memset(&ev, 0, sizeof ev);
    ev.kind = WET_EVENT_BLUR; ev.target = app->focus; ev.widget = app->focus;
    if (app->focus) wet_dispatch(app, app->focus, &ev);
    app->focus = w;
    if (w) {
        ev.kind = WET_EVENT_FOCUS; ev.target = w; ev.widget = w;
        wet_dispatch(app, w, &ev);
    }
}

void wet_widget_on(WetWidget *w, WetEventKind kind, WetHandler fn, void *ud) {
    if (!w || kind <= WET_EVENT_NONE || kind >= WET_EVENT_COUNT) return;
    w->handlers[kind] = (WetHandlerSlot){ fn, ud, true };
}

/* -------------------------------------------------------------- dispatch */

int wet_dispatch(WetApp *app, WetWidget *w, const WetEvent *ev) {
    if (!w || !ev) return 0;
    const WetHandlerSlot *s = &w->handlers[ev->kind];
    if (s->used && s->fn) {
        if (s->fn(app, w, ev, s->ud)) return 1;
    }
    /* Script handlers run before the event bubbles, so a .wet applet sees
     * the event at the control it came from. */
    if (app && app->lua_dispatch)
        app->lua_dispatch(app->lua_ctx, w, ev);

    /* Bubbles to the parent, the .NET route-event way. */
    return w->parent ? wet_dispatch(app, w->parent, ev) : 0;
}

/* ------------------------------------------------------------------ index */

void wet_app_index(WetApp *app, WetWidget *w) {
    if (!app || !w || !w->id) return;
    if (app->index_n == app->index_cap) {
        int cap = app->index_cap ? app->index_cap * 2 : 16;
        WetWidget **grown = (WetWidget **)realloc(app->index,
                                                  (size_t)cap * sizeof *grown);
        if (!grown) return;
        app->index = grown;
        app->index_cap = cap;
    }
    app->index[app->index_n++] = w;
}

WetWidget *wet_app_lookup(WetApp *app, const char *id) {
    if (!app || !id) return NULL;
    for (int i = 0; i < app->index_n; i++)
        if (app->index[i]->id && strcmp(app->index[i]->id, id) == 0)
            return app->index[i];
    return NULL;
}
