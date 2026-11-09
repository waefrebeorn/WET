/*
 * wet.h -- the .wet framework: a .NET-style applet stack for WuBuOS.
 *
 * ".wet" is WuBu's .net: a declarative widget tree laid out by a real
 * measure/arrange pass (the WPF model, not a hand-placed pile of rects) and
 * rendered through the VBE, so a .wet applet composites with the same shell
 * as a C app.
 *
 * The model, in .NET terms:
 *
 *   WetApp      = Application.  Owns the widget tree, the input focus and
 *                 the render backend.  One per applet.
 *   WetWidget   = FrameworkElement.  Has an id, a desired size, a final
 *                 rect, a style, and event handlers.
 *   Stack/Dock/Canvas/Grid = Panel.  They measure and arrange children.
 *   Label/Button/... = Control.
 *
 * Layout is two-pass.  A panel asks each child "how big do you want to be?"
 * (measure) and then says "here is what you get" (arrange).  A control never
 * positions itself -- the same script runs identically at any window size.
 *
 * Lua owns the tree: a .wet file calls wet.applet{...} and the host walks the
 * table.  A script can also reach a widget from a handler via wet.find("id").
 *
 * Include this header from C only; the Lua binding lives in wet_lua.c.
 */
#ifndef WET_H
#define WET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WetApp   WetApp;
typedef struct WetWidget WetWidget;

/* ------------------------------------------------------------- geometry */

typedef struct { int x, y, w, h; } WetRect;
typedef struct { int w, h; }     WetSize;

#define WET_RECT_EMPTY ((WetRect){0,0,0,0})

static inline int wet_rect_right  (WetRect r) { return r.x + r.w; }
static inline int wet_rect_bottom (WetRect r) { return r.y + r.h; }
static inline int wet_rect_contains(WetRect r, int px, int py) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}
static inline WetRect wet_rect_clip(WetRect a, WetRect b) {
    int x1 = a.x > b.x ? a.x : b.x;
    int y1 = a.y > b.y ? a.y : b.y;
    int x2 = wet_rect_right(a)  < wet_rect_right(b)  ? wet_rect_right(a)  : wet_rect_right(b);
    int y2 = wet_rect_bottom(a) < wet_rect_bottom(b) ? wet_rect_bottom(a) : wet_rect_bottom(b);
    WetRect r = { x1, y1, x2 - x1, y2 - y1 };
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}

/* --------------------------------------------------------------- colours */

/* 0xRRGGBB, matching the VBE's colour words. */
typedef uint32_t WetColor;

#define WET_RGB(R,G,B) ((WetColor)(((R) << 16) | ((G) << 8) | (B)))
#define WET_BLACK   0x000000u
#define WET_WHITE   0xFFFFFFu
#define WET_SILVER  0xC0C0C0u
#define WET_GRAY    0x808080u
#define WET_NAVY    0x000080u
#define WET_BLUE    0x0000FFu
#define WET_RED     0xFF0000u
#define WET_GREEN   0x008000u
#define WET_YELLOW  0xFFFF00u

/* ---------------------------------------------------------------- events */

typedef enum {
    WET_EVENT_NONE = 0,
    WET_EVENT_CLICK,        /* a control was activated            */
    WET_EVENT_DBLCLICK,
    WET_EVENT_CHANGE,       /* value changed (slider, check...)   */
    WET_EVENT_TEXTCHANGED,  /* a text box's content changed        */
    WET_EVENT_SELECT,       /* a list selection changed           */
    WET_EVENT_FOCUS,        /* gained keyboard focus              */
    WET_EVENT_BLUR,         /* lost keyboard focus                */
    WET_EVENT_KEYDOWN,
    WET_EVENT_KEYUP,
    WET_EVENT_MOUSEDOWN,
    WET_EVENT_MOUSEUP,
    WET_EVENT_MOUSEMOVE,
    WET_EVENT_PAINT,        /* custom paint; last chance to draw  */
    WET_EVENT_COUNT
} WetEventKind;

/* Handler signature.  Return 1 if the event was consumed. */
typedef int (*WetHandler)(WetApp *app, WetWidget *w, const void *ev, void *ud);

/* ------------------------------------------------------------- lifecycle */

WetApp   *wet_app_new(int width, int height);
void      wet_app_free(WetApp *app);

/* Run the two-pass layout and draw the tree through the app's backend. */
void      wet_app_layout(WetApp *app);
void      wet_app_render(WetApp *app);

/* Resize every time the window does; re-runs the layout pass. */
void      wet_app_resize(WetApp *app, int width, int height);
void      wet_app_root(WetApp *app, WetWidget *w);

WetWidget *wet_app_find(const WetApp *app, const char *id);
void      *wet_app_data(WetApp *app);

/* Report and read the applet's last error.  A raising script handler or a
 * rejected table lands here; it never terminates the shell. */
const char *wet_app_error(WetApp *app, const char *msg);
const char *wet_app_last_error(const WetApp *app);

/* -------------------------------------------------------------- widgets */

/* These are the types a .wet script can name.  Every one is a class name
 * registered with wet_class_register(); unknown names are a hard error. */
typedef enum {
    WET_CLASS_LABEL = 1,
    WET_CLASS_BUTTON,
    WET_CLASS_TEXTBOX,
    WET_CLASS_CHECKBOX,
    WET_CLASS_LISTBOX,
    WET_CLASS_PROGRESS,
    WET_CLASS_SLIDER,
    WET_CLASS_SEPARATOR,
    WET_CLASS_GROUPBOX,
    WET_CLASS_IMAGE,
    WET_CLASS_STACK,
    WET_CLASS_DOCK,
    WET_CLASS_CANVAS,
    WET_CLASS_GRID,
    WET_CLASS_CUSTOM
} WetClassId;

typedef struct WetWidgetClass WetWidgetClass;

WetWidget *wet_widget_new(WetApp *app, WetClassId cls, const char *id);
void       wet_widget_free(WetWidget *w);

WetWidget *wet_widget_parent(const WetWidget *w);
WetWidget *wet_widget_child (const WetWidget *w, int index);
int        wet_widget_nchildren(const WetWidget *w);
void       wet_widget_add   (WetWidget *parent, WetWidget *child);
void       wet_widget_remove(WetWidget *parent, WetWidget *child);

const char     *wet_widget_id    (const WetWidget *w);
WetClassId      wet_widget_class(const WetWidget *w);
WetRect         wet_widget_rect (const WetWidget *w);
WetSize         wet_widget_desired(const WetWidget *w);

void wet_widget_set_visible(WetWidget *w, bool visible);
bool wet_widget_visible(const WetWidget *w);
void wet_widget_set_enabled(WetWidget *w, bool enabled);
bool wet_widget_enabled(const WetWidget *w);
void wet_widget_set_focus(WetApp *app, WetWidget *w);

/* Per-type payload.  Cast according to wet_widget_class().  The structs are
 * declared in wet_widget.h; a script never sees them. */
void *wet_widget_payload(WetWidget *w);

/* ---------------------------------------------------------- control API */
/* Each is a thin, type-checked accessor over the payload. */

const char *wet_label_text   (const WetWidget *w);
void        wet_label_set_text(WetWidget *w, const char *text);

const char *wet_button_text  (const WetWidget *w);
void        wet_button_set_text(WetWidget *w, const char *text);

const char *wet_textbox_text (const WetWidget *w);
void        wet_textbox_set_text(WetWidget *w, const char *text);
bool        wet_textbox_password(const WetWidget *w);
void        wet_textbox_set_password(WetWidget *w, bool on);

bool        wet_checkbox_checked(const WetWidget *w);
void        wet_checkbox_set_checked(WetWidget *w, bool on);

int         wet_listbox_count  (const WetWidget *w);
const char *wet_listbox_item   (const WetWidget *w, int index);
int         wet_listbox_select (const WetWidget *w);
void        wet_listbox_add    (WetWidget *w, const char *text);
void        wet_listbox_set_select(WetWidget *w, int index);

int         wet_progress_value(const WetWidget *w);
void        wet_progress_set_value(WetWidget *w, int v);
int         wet_progress_maximum(const WetWidget *w);
void        wet_progress_set_maximum(WetWidget *w, int v);

int         wet_slider_value  (const WetWidget *w);
void        wet_slider_set_value(WetWidget *w, int v);
int         wet_slider_minimum(const WetWidget *w);
int         wet_slider_maximum(const WetWidget *w);
void        wet_slider_set_minimum(WetWidget *w, int v);
void        wet_slider_set_maximum(WetWidget *w, int v);

int         wet_image_width (const WetWidget *w);
int         wet_image_height(const WetWidget *w);

/* --------------------------------------------------------- layout API */

typedef enum {
    WET_ORIENT_VERTICAL = 0,
    WET_ORIENT_HORIZONTAL
} WetOrient;

WetOrient wet_stack_orient(const WetWidget *w);
void      wet_stack_set_orient(WetWidget *w, WetOrient o);
int       wet_stack_spacing(const WetWidget *w);
void      wet_stack_set_spacing(WetWidget *w, int px);

/* Dock: which edge a child was docked to. */
typedef enum {
    WET_DOCK_NONE = 0, WET_DOCK_LEFT, WET_DOCK_TOP,
    WET_DOCK_RIGHT, WET_DOCK_BOTTOM, WET_DOCK_FILL
} WetDock;
WetDock wet_dock_get(const WetWidget *child);
void    wet_dock_set(WetWidget *child, WetDock dock);

/* Canvas: absolute position, in the .NET sense. */
int  wet_canvas_left(const WetWidget *child);
int  wet_canvas_top (const WetWidget *child);
void wet_canvas_set_left(WetWidget *child, int x);
void wet_canvas_set_top (WetWidget *child, int y);

/* Grid: row/column, 0-based. */
int  wet_grid_col(const WetWidget *child);
int  wet_grid_row(const WetWidget *child);
void wet_grid_set_col(WetWidget *child, int c);
void wet_grid_set_row(WetWidget *child, int r);
int  wet_grid_columns(const WetWidget *w);
int  wet_grid_rows   (const WetWidget *w);
void wet_grid_set_columns(WetWidget *w, int n);
void wet_grid_set_rows   (WetWidget *w, int n);

/* ------------------------------------------------------------ events API */

void wet_widget_on(WetWidget *w, WetEventKind kind, WetHandler fn, void *ud);
/* Convenience for the Lua binding: handlers are stored in the app's handler
 * table, not in the widget, so a script can attach them after construction. */

/* ------------------------------------------------------------- rendering */

typedef struct WetRender WetRender;   /* wet_render.h */

/* --------------------------------------------------------- application */

/* Load and build a .wet applet from a Lua source string.  Returns NULL on
 * success or a static error string.  The script must call wet.applet{...};
 * see wet_lua.c. */
const char *wet_app_load(WetApp *app, const char *src, const char *chunkname);

/* Render backend used by wet_app_render().  wet_render_vbe returns one that
 * draws through the VBE; tests supply their own. */
WetRender *wet_render_vbe(void);

/* Input routing.  Pointer coordinates are applet-local.  Both return 1 if
 * something in the tree consumed the event. */
int wet_app_pointer(WetApp *app, WetEventKind kind, int x, int y, int button);
int wet_app_key(WetApp *app, WetEventKind kind, int key, uint32_t mods);

#ifdef __cplusplus
}
#endif

#endif /* WET_H */
