/*
 * wet_internal.h -- .wet internals.  Not for inclusion outside src/gui/wet.
 *
 * Split by concern so each .c owns one: the widget tree (wet_widget.c), the
 * panels (wet_layout.c), the drawing (wet_render.c), the input routing
 * (wet_input.c), the control payloads (wet_controls.c), the Lua binding
 * (wet_lua.c), and the applet host (wet_app.c).
 */
#ifndef WET_INTERNAL_H
#define WET_INTERNAL_H

#include "wet.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* --------------------------------------------------------------- payloads */

typedef struct {
    char    *text;
    WetColor color;
    bool     wrap;
} WetLabel;

typedef struct {
    char    *text;
    WetColor color;
    bool     default_button;
} WetButton;

typedef struct {
    char    *text;
    bool     password;
    bool     multiline;
    int      caret;
    int      scroll;
} WetTextBox;

typedef struct {
    char    *text;
    bool     checked;
    bool     tri_state;
} WetCheckBox;

typedef struct {
    char   **items;
    int      nitems;
    int      cap;
    int      selected;
    int      scroll;
} WetListBox;

typedef struct {
    int value;
    int maximum;
    int minimum;
} WetProgress;

typedef struct {
    int value;
    int minimum;
    int maximum;
    int step;
} WetSlider;

typedef struct {
    bool vertical;
} WetSeparator;

typedef struct {
    char *text;
} WetGroupBox;

typedef struct {
    int   width;
    int   height;
    WetColor *pixels;      /* width*height, 0xRRGGBB, may be NULL */
    char *path;
} WetImage;

typedef struct {
    WetOrient orient;
    int       spacing;
    bool      expand;      /* stretch children along the stack axis */
} WetStack;

typedef struct {
    /* children carry their own WetDock in WetDockSlot */
} WetDockPanel;

typedef struct {
    /* children carry their own WetCanvasPos */
} WetCanvasPanel;

typedef struct {
    int cols, rows;
    int col_gap, row_gap;
} WetGridPanel;

typedef struct {
    /* A designer sets an explicit size; the canvas then honours it instead of
     * the control's measured desired size. */
    bool      size_set;
    int       size_w, size_h;
    WetDock   dock;
    int       canvas_left, canvas_top;
    int       grid_col, grid_row;
    int       margin_l, margin_t, margin_r, margin_b;
    int       halign, valign;
} WetLayoutSlot;

/* --------------------------------------------------------------- widget */

#define WET_MAX_HANDLERS 8

typedef struct WetHandlerSlot {
    WetHandler fn;
    void      *ud;
    bool      used;
} WetHandlerSlot;

struct WetWidget {
    WetApp        *app;
    const WetWidgetClass *cls;
    char          *id;

    WetWidget     *parent;
    WetWidget    **children;
    int            nchildren;
    int            capchildren;

    WetLayoutSlot  slot;
    WetRect        rect;        /* final, after arrange            */
    WetSize        desired;     /* what measure() asked for        */
    bool           visible;
    bool           enabled;
    bool           focused;

    void          *payload;

    WetHandlerSlot handlers[WET_EVENT_COUNT];

    /* Set by a panel when it wants the child to fill the remaining space. */
    bool           expand_h, expand_v;
};

/* --------------------------------------------------------------- classes */

struct WetWidgetClass {
    const char  *name;
    WetClassId   id;
    size_t       payload_size;
    void       (*init)   (WetWidget *w);
    void       (*measure)(WetWidget *w, WetSize avail, WetSize *desired);
    void       (*arrange)(WetWidget *w, WetRect r);
    void       (*render)(WetWidget *w, WetRender *r);
    int        (*input) (WetWidget *w, const void *ev);
    void       (*destroy)(WetWidget *w);
};

void wet_class_register(const WetWidgetClass *cls);
const WetWidgetClass *wet_class_lookup(WetClassId id);
const WetWidgetClass *wet_class_by_name(const char *name);

/* Shared text measurement: a control measures itself from its text. */
int  wet_text_width (const char *s);
int  wet_text_height(void);
void wet_text_draw  (WetRender *r, int x, int y, const char *s, WetColor c);

/* Theme: the palette a control paints with.  `name` is set by the registry so
 * wet_theme_name() can report what is active; a theme built by hand leaves it
 * NULL. */
typedef struct {
    const char *name;
    WetColor window;      /* applet background          */
    WetColor surface;     /* panel / control face       */
    WetColor sunken;      /* text box interior          */
    WetColor border;      /* 3-D border                 */
    WetColor border_lit;  /* lit edge                   */
    WetColor border_shade;/* shaded edge                */
    WetColor text;
    WetColor text_dim;
    WetColor accent;
    WetColor accent_text;
    WetColor selection;
    WetColor selection_text;
    int      font_scale;
} WetTheme;

extern const WetTheme wet_theme_default;
extern const WetTheme wet_theme_win98;
extern const WetTheme wet_theme_xp;
extern const WetTheme wet_theme_wubu;
extern const WetTheme wet_theme_dark;

const WetTheme *wet_theme_get(void);
void wet_theme_set(const WetTheme *t);

/* The named themes, in picker order.  wet_theme_by_name returns NULL for an
 * unknown name.  wet_theme_name returns the active theme's name, or "custom". */
int                wet_theme_count(void);
const WetTheme    *wet_theme_at(int i);
const char        *wet_theme_name_at(int i);
const WetTheme    *wet_theme_by_name(const char *name);
const char        *wet_theme_name(void);

/* --------------------------------------------------------------- render */

struct WetRender {
    void *ctx;
    void (*clear )(void *ctx, WetColor color);
    void (*fill  )(void *ctx, int x, int y, int w, int h, WetColor color);
    void (*rect  )(void *ctx, int x, int y, int w, int h, WetColor color);
    void (*hline )(void *ctx, int x1, int x2, int y, WetColor color);
    void (*text  )(void *ctx, int x, int y, const char *s, WetColor color);
    int  (*text_width )(void *ctx, const char *s);
    int  (*text_height)(void *ctx);
};

/* Primitives every control is built from.  They clip and clip-check. */
void wet_r_fill   (WetRender *r, WetRect bounds, WetRect g, WetColor c);
void wet_r_rect   (WetRender *r, WetRect bounds, WetRect g, WetColor c);
void wet_r_text   (WetRender *r, WetRect bounds, int x, int y, const char *s, WetColor c);
void wet_r_raised (WetRender *r, WetRect bounds, WetRect g, WetColor face);
void wet_r_sunken (WetRender *r, WetRect bounds, WetRect g, WetColor face);

/* --------------------------------------------------------------- events */

typedef struct WetEvent {
    WetEventKind kind;
    WetWidget   *widget;
    WetWidget   *target;
    int          x, y;         /* widget-local                      */
    int          button;
    int          key;
    uint32_t     mods;
    const char  *text;
    int          value;
} WetEvent;

int wet_dispatch(WetApp *app, WetWidget *w, const WetEvent *ev);

/* --------------------------------------------------------------- app */

#define WET_MAX_ID 64
#define WET_MAX_WIDGETS 512

struct WetApp {
    WetWidget  *root;
    WetWidget  *focus;
    int         width, height;
    int         origin_x, origin_y;   /* where applet (0,0) lands on screen */
    WetRender  *render;
    void       *userdata;

    /* id -> widget, for wet.find() */
    WetWidget **index;
    int         index_n;
    int         index_cap;

    /* Optional Lua bridge.  NULL for a pure-C applet, so the framework has
     * no hard dependency on the interpreter. */
    void       *L;
    void       *lua_ctx;
    void      (*lua_dispatch)(void *lua_ctx, WetWidget *, const WetEvent *);
};

WetWidget *wet_app_lookup(WetApp *app, const char *id);
void       wet_app_index(WetApp *app, WetWidget *w);

/* --------------------------------------------------------------- panels */

/* Shared measure/arrange helpers. */
void wet_panel_measure_children(WetWidget *w, WetSize avail, WetSize *desired);
int  wet_panel_gap(const WetWidget *w);

/* Run one widget's arrange pass and its render pass.  Panels call these on
 * their children; controls never call them on themselves. */
void wet_widget_arrange(WetWidget *w, WetRect r);
void wet_widget_render(WetWidget *w, WetRender *r);

/* Internal: recursive measure. */
void wet_widget_measure(WetWidget *w, WetSize avail);

#ifdef __cplusplus
}
#endif

#endif /* WET_INTERNAL_H */
