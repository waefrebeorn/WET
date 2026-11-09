/*
 * wet_lua.c -- the Lua half of the .wet framework.
 *
 * A .wet applet is a Lua script that calls wet.applet{...}.  The script builds
 * the tree by calling constructors -- wet.stack{ wet.label{text="A"} } -- and
 * the host walks the resulting tables, creating a real WetWidget for each one.
 * Numeric keys in a table are children, which is the Lua idiom for a panel's
 * content and reads like XAML's nested elements.
 *
 * Event handlers are plain Lua functions.  The host stores them by value and
 * calls them through wb_lv_pcall, so a raising script cannot take the shell
 * down.  A handler receives `self`, a proxy whose metatable reads and writes
 * the widget's real properties -- so `wet.find("name").text = "x"` works
 * exactly as it reads.
 */
#include "wet_internal.h"
#include "wet_lua.h"

#include "wb_lv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------------------------------------------------------------- context */

typedef struct WetLuaCtx WetLuaCtx;

struct WetLua { WetLuaCtx *ctx; };

typedef struct WetHandlerRef {
    WetWidget   *w;
    WetEventKind kind;
    wb_lv_value  fn;
} WetHandlerRef;

struct WetLuaCtx {
    wb_lv    *L;
    WetApp   *app;

    /* Stored Lua handlers: widget + kind + function value. */
    WetHandlerRef *handlers;
    int nhandlers, caphandlers;

    /* Metatable for widget proxies, and the property accessor tables. */
    wb_lv_table *proxy_meta;
    wb_lv_table *proxy_index;      /* name -> getter C function */
    wb_lv_table *proxy_newindex;   /* name -> setter C function */
};

static WetLuaCtx *g_ctx;   /* one applet per process is the .wet model */

/* --------------------------------------------------------------- helpers */

static wb_lv_value str(const char *s) { return wb_lv_str(s); }

/* A key that must match a script's own literal.  The interpreter hashes table
 * keys by string identity, so a plain wb_lv_str("id") never finds the "id"
 * the lexer interned.  Intern every key we look up in a script table. */
static wb_lv_value k(wb_lv *L, const char *s) {
    return wb_lv_str(wb_lv_intern(L, s, strlen(s)));
}

static const char *kind_name(WetEventKind k) {
    switch (k) {
    case WET_EVENT_CLICK:        return "click";
    case WET_EVENT_DBLCLICK:     return "dblclick";
    case WET_EVENT_CHANGE:       return "change";
    case WET_EVENT_TEXTCHANGED:  return "textchanged";
    case WET_EVENT_SELECT:       return "select";
    case WET_EVENT_FOCUS:        return "focus";
    case WET_EVENT_BLUR:         return "blur";
    case WET_EVENT_KEYDOWN:      return "keydown";
    case WET_EVENT_KEYUP:        return "keyup";
    case WET_EVENT_MOUSEDOWN:    return "mousedown";
    case WET_EVENT_MOUSEUP:      return "mouseup";
    case WET_EVENT_MOUSEMOVE:    return "mousemove";
    case WET_EVENT_PAINT:        return "paint";
    default:                     return NULL;
    }
}

static WetEventKind kind_from(const char *name) {
    for (int k = 1; k < WET_EVENT_COUNT; k++)
        if (strcmp(kind_name((WetEventKind)k), name) == 0) return (WetEventKind)k;
    return WET_EVENT_NONE;
}

/* Get a widget pointer from a proxy table's hidden field. */
static WetWidget *proxy_widget(wb_lv *L, wb_lv_table *t) {
    if (!t) return NULL;
    wb_lv_value v = wb_lv_table_get(L, t, k(L, "_wet_ptr"));
    if (v.t == WB_LV_INT) return (WetWidget *)(intptr_t)v.as.i;
    return NULL;
}

static void proxy_set_ptr(wb_lv *L, wb_lv_table *t, WetWidget *w) {
    if (!t) return;
    wb_lv_table_set(L, t, k(L, "_wet_ptr"),
                    wb_lv_integer((int64_t)(intptr_t)w));
}

/* --------------------------------------------------------------- builder */

typedef struct {
    WetLuaCtx   *ctx;
    const char  *cls;       /* table key holding the class name */
    WetClassId   id;
} BuildErr;

static WetClassId class_from_name(const char *name) {
    if (!name) return 0;
    struct { const char *n; WetClassId id; } tab[] = {
        {"label", WET_CLASS_LABEL},       {"button", WET_CLASS_BUTTON},
        {"textbox", WET_CLASS_TEXTBOX},   {"checkbox", WET_CLASS_CHECKBOX},
        {"listbox", WET_CLASS_LISTBOX},   {"progress", WET_CLASS_PROGRESS},
        {"slider", WET_CLASS_SLIDER},     {"separator", WET_CLASS_SEPARATOR},
        {"groupbox", WET_CLASS_GROUPBOX}, {"image", WET_CLASS_IMAGE},
        {"stack", WET_CLASS_STACK},       {"dock", WET_CLASS_DOCK},
        {"canvas", WET_CLASS_CANVAS},     {"grid", WET_CLASS_GRID},
        {NULL, 0}
    };
    for (int i = 0; tab[i].n; i++)
        if (strcmp(tab[i].n, name) == 0) return tab[i].id;
    return 0;
}

/* Apply a string property. */
static void set_string_prop(WetLuaCtx *ctx, WetWidget *w, const char *key,
                            const char *val) {
    if (strcmp(key, "text") == 0) {
        switch (w->cls->id) {
        case WET_CLASS_LABEL:    wet_label_set_text(w, val); return;
        case WET_CLASS_BUTTON:   wet_button_set_text(w, val); return;
        case WET_CLASS_TEXTBOX:  wet_textbox_set_text(w, val); return;
        case WET_CLASS_CHECKBOX: {
            WetCheckBox *c = (WetCheckBox *)w->payload; free(c->text);
            c->text = strdup(val ? val : ""); return;
        }
        case WET_CLASS_GROUPBOX: {
            WetGroupBox *g = (WetGroupBox *)w->payload; free(g->text);
            g->text = strdup(val ? val : ""); return;
        }
        default: return;
        }
    }
    (void)ctx;
}

static void set_int_prop(WetWidget *w, const char *key, int v) {
    if (strcmp(key, "spacing") == 0)   { wet_stack_set_spacing(w, v); return; }
    if (strcmp(key, "checked") == 0)   { wet_checkbox_set_checked(w, v != 0); return; }
    if (strcmp(key, "value") == 0) {
        if (w->cls->id == WET_CLASS_PROGRESS) { wet_progress_set_value(w, v); return; }
        if (w->cls->id == WET_CLASS_SLIDER)   { wet_slider_set_value(w, v); return; }
        return;
    }
    if (strcmp(key, "maximum") == 0) {
        if (w->cls->id == WET_CLASS_PROGRESS) { wet_progress_set_maximum(w, v); return; }
        if (w->cls->id == WET_CLASS_SLIDER)   { wet_slider_set_maximum(w, v); return; }
        return;
    }
    if (strcmp(key, "minimum") == 0) {
        if (w->cls->id == WET_CLASS_SLIDER) { wet_slider_set_minimum(w, v); return; }
        return;
    }
    if (strcmp(key, "selected") == 0)  { wet_listbox_set_select(w, v); return; }
    if (strcmp(key, "password") == 0)  { wet_textbox_set_password(w, v != 0); return; }
    if (strcmp(key, "columns") == 0)   { wet_grid_set_columns(w, v); return; }
    if (strcmp(key, "rows") == 0)      { wet_grid_set_rows(w, v); return; }
    if (strcmp(key, "dock") == 0) {
        static const WetDock d[] = { WET_DOCK_NONE, WET_DOCK_LEFT, WET_DOCK_TOP,
                                     WET_DOCK_RIGHT, WET_DOCK_BOTTOM, WET_DOCK_FILL };
        if (v >= 0 && v < 6) wet_dock_set(w, d[v]);
        return;
    }
    if (strcmp(key, "left") == 0)    { wet_canvas_set_left(w, v); return; }
    if (strcmp(key, "top") == 0)     { wet_canvas_set_top(w, v); return; }
    if (strcmp(key, "col") == 0)     { wet_grid_set_col(w, v); return; }
    if (strcmp(key, "row") == 0)     { wet_grid_set_row(w, v); return; }
}

/* Build one widget from a table.  Returns NULL and fills err on failure. */
static WetWidget *build_widget(WetLuaCtx *ctx, wb_lv_table *t, const char **err) {
    wb_lv *L = ctx->L;
    WetWidget *w = NULL;

    wb_lv_value cls = wb_lv_table_get(L, t, k(L, "__wet"));
    if (cls.t != WB_LV_STR) { *err = "element has no class marker"; return NULL; }

    const char *cname = wb_lv_tostring(cls);
    WetClassId id = class_from_name(cname);
    if (!id) { static char bad[64]; snprintf(bad, sizeof bad, "unknown element '%s'", cname);
               *err = bad; return NULL; }

    wb_lv_value idv = wb_lv_table_get(L, t, k(L, "id"));
    const char *wid = (idv.t == WB_LV_STR) ? wb_lv_tostring(idv) : NULL;

    w = wet_widget_new(ctx->app, id, wid);
    if (!w) { *err = "out of memory"; return NULL; }

    /* Children.  The reliable form is an explicit array:
     *
     *   wet.stack { children = { wet.label{...}, wet.label{...} } }
     *
     * Numeric keys work too (wet.stack { wet.label{...}, ... }).  Wua fixed
     * the table-constructor bug that used to drop the array part when a keyed
     * field came first, so both forms are now reliable; `children` still wins
     * when both are present. */
    bool have_children = false;
    wb_lv_value cv = wb_lv_table_get(L, t, k(L, "children"));
    if (cv.t == WB_LV_TABLE) {
        size_t cn = wb_lv_table_len(cv.as.tab);
        for (size_t i = 1; i <= cn; i++) {
            wb_lv_value v = wb_lv_table_get(L, cv.as.tab, wb_lv_integer((int64_t)i));
            if (v.t != WB_LV_TABLE) continue;
            if (wb_lv_table_get(L, v.as.tab, k(L, "__wet")).t != WB_LV_STR) continue;
            const char *cerr = NULL;
            WetWidget *child = build_widget(ctx, v.as.tab, &cerr);
            if (!child) { *err = cerr; wet_widget_free(w); return NULL; }
            wet_widget_add(w, child);
            have_children = true;
        }
    }
    if (!have_children) {
        size_t n = wb_lv_table_len(t);
        for (size_t i = 1; i <= n; i++) {
            wb_lv_value v = wb_lv_table_get(L, t, wb_lv_integer((int64_t)i));
            if (v.t != WB_LV_TABLE) continue;
            if (wb_lv_table_get(L, v.as.tab, k(L, "__wet")).t != WB_LV_STR) continue;
            const char *cerr = NULL;
            WetWidget *child = build_widget(ctx, v.as.tab, &cerr);
            if (!child) { *err = cerr; wet_widget_free(w); return NULL; }
            wet_widget_add(w, child);
        }
    }

    /* Named properties.  We enumerate by a fixed list, which keeps the
     * binding narrow: only these names mean anything. */
    static const char *props[] = {
        "text", "spacing", "checked", "value", "maximum", "minimum",
        "selected", "password", "columns", "rows", "dock", "left", "top",
        "col", "row", "orient", "expand", "wrap", NULL
    };
    for (int i = 0; props[i]; i++) {
        wb_lv_value v = wb_lv_table_get(L, t, k(L, props[i]));
        if (v.t == WB_LV_NIL) continue;
        if (v.t == WB_LV_STR) {
            set_string_prop(ctx, w, props[i], wb_lv_tostring(v));
        } else if (v.t == WB_LV_INT) {
            set_int_prop(w, props[i], (int)v.as.i);
        } else if (v.t == WB_LV_BOOL) {
            set_int_prop(w, props[i], v.as.b ? 1 : 0);
        }
    }

    /* An explicit designer size (a form designer places a button at exactly
     * 100x28).  The canvas honours it over the control's measured size. */
    {
        wb_lv_value wv = wb_lv_table_get(L, t, k(L, "w"));
        wb_lv_value hv = wb_lv_table_get(L, t, k(L, "h"));
        if (wv.t == WB_LV_INT && hv.t == WB_LV_INT)
            wet_canvas_set_size(w, (int)wv.as.i, (int)hv.as.i);
    }

    /* Event handlers. */
    for (int ev = 1; ev < WET_EVENT_COUNT; ev++) {
        const char *base = kind_name((WetEventKind)ev);
        if (!base) continue;
        char kn[32];
        snprintf(kn, sizeof kn, "on%s", base);
        wb_lv_value fn = wb_lv_table_get(L, t, k(L, kn));
        if (fn.t != WB_LV_FUNCTION && fn.t != WB_LV_CFUNCTION) continue;
        if (ctx->nhandlers == ctx->caphandlers) {
            int cap = ctx->caphandlers ? ctx->caphandlers * 2 : 16;
            WetHandlerRef *grown = (WetHandlerRef *)realloc(ctx->handlers,
                                                            (size_t)cap * sizeof *grown);
            if (!grown) continue;
            ctx->handlers = grown;
            ctx->caphandlers = cap;
        }
        ctx->handlers[ctx->nhandlers].w    = w;
        ctx->handlers[ctx->nhandlers].kind = (WetEventKind)ev;
        ctx->handlers[ctx->nhandlers].fn   = fn;
        ctx->nhandlers++;
    }

    return w;
}

/* --------------------------------------------------------------- events */

/* Look up the C handler for a widget+kind, then call its Lua function. */
static void call_handlers(WetLuaCtx *ctx, WetWidget *w, const WetEvent *ev) {
    wb_lv *L = ctx->L;
    for (int i = 0; i < ctx->nhandlers; i++) {
        if (ctx->handlers[i].w != w || ctx->handlers[i].kind != ev->kind) continue;

        /* self = a proxy table for this widget */
        wb_lv_table *proxy = wb_lv_table_new(L, 0, 2);
        proxy_set_ptr(L, proxy, w);
        wb_lv_setmetatable(L, proxy, ctx->proxy_meta);

        /* ev = a table the script can read */
        wb_lv_table *tev = wb_lv_table_new(L, 0, 4);
        wb_lv_table_set(L, tev, k(L, "kind"), str(kind_name(ev->kind)));
        wb_lv_table_set(L, tev, k(L, "x"), wb_lv_integer(ev->x));
        wb_lv_table_set(L, tev, k(L, "y"), wb_lv_integer(ev->y));
        wb_lv_table_set(L, tev, k(L, "value"), wb_lv_integer(ev->value));
        if (ev->text) wb_lv_table_set(L, tev, k(L, "text"), str(ev->text));

        wb_lv_value args[2];
        args[0].t = WB_LV_TABLE; args[0].as.tab = proxy;
        args[1].t = WB_LV_TABLE; args[1].as.tab = tev;

        wb_lv_value out[8]; int nout = 0;
        int rc = wb_lv_pcall(L, ctx->handlers[i].fn, args, 2, out, &nout);
        if (rc != 0) {
            /* wb_lv_pcall leaves nout at 0 on error; the message is stashed
             * in __lv_last_error rather than returned in out[]. */
            wb_lv_value errv = wb_lv_get_global(L, "__lv_last_error");
            const char *e = (errv.t == WB_LV_STR) ? wb_lv_tostring(errv) : NULL;
            char buf[256];
            snprintf(buf, sizeof buf, "wet: handler '%s' raised: %s",
                     kind_name(ev->kind), e ? e : "unknown error");
            wet_app_error(ctx->app, buf);
        }
    }
}

/* --------------------------------------------------------------- proxies */

static wb_lv_value proxy_index_fn(wb_lv *L, wb_lv_value *args, int n, void *ud) {
    WetLuaCtx *ctx = (WetLuaCtx *)ud;
    if (n < 2) return wb_lv_nil();
    WetWidget *w = proxy_widget(L, args[0].as.tab);
    if (!w) return wb_lv_nil();
    const char *key = wb_lv_tostring(args[1]);
    if (!key) return wb_lv_nil();

    if (strcmp(key, "text") == 0) {
        const char *s = NULL;
        switch (w->cls->id) {
        case WET_CLASS_LABEL:   s = wet_label_text(w); break;
        case WET_CLASS_BUTTON:  s = wet_button_text(w); break;
        case WET_CLASS_TEXTBOX: s = wet_textbox_text(w); break;
        case WET_CLASS_CHECKBOX: {
            WetCheckBox *c = (WetCheckBox *)w->payload; s = c->text; break;
        }
        case WET_CLASS_GROUPBOX: {
            WetGroupBox *g = (WetGroupBox *)w->payload; s = g->text; break;
        }
        default: break;
        }
        return s ? str(s) : wb_lv_nil();
    }
    if (strcmp(key, "checked") == 0)   return wb_lv_bool(wet_checkbox_checked(w));
    if (strcmp(key, "value") == 0) {
        if (w->cls->id == WET_CLASS_PROGRESS) return wb_lv_integer(wet_progress_value(w));
        if (w->cls->id == WET_CLASS_SLIDER)   return wb_lv_integer(wet_slider_value(w));
        return wb_lv_nil();
    }
    if (strcmp(key, "maximum") == 0)    return wb_lv_integer(wet_progress_maximum(w));
    if (strcmp(key, "minimum") == 0)    return wb_lv_integer(wet_slider_minimum(w));
    if (strcmp(key, "selected") == 0)   return wb_lv_integer(wet_listbox_select(w));
    if (strcmp(key, "count") == 0)      return wb_lv_integer(wet_listbox_count(w));
    if (strcmp(key, "password") == 0)   return wb_lv_bool(wet_textbox_password(w));
    if (strcmp(key, "id") == 0)         return w->id ? str(w->id) : wb_lv_nil();
    if (strcmp(key, "visible") == 0)    return wb_lv_bool(wet_widget_visible(w));
    if (strcmp(key, "enabled") == 0)    return wb_lv_bool(wet_widget_enabled(w));
    if (strcmp(key, "spacing") == 0)    return wb_lv_integer(wet_stack_spacing(w));
    if (strcmp(key, "columns") == 0)    return wb_lv_integer(wet_grid_columns(w));
    if (strcmp(key, "rows") == 0)       return wb_lv_integer(wet_grid_rows(w));
    if (strcmp(key, "item") == 0) {
        /* item(i) -- a method call, so args[1] is the index */
        int ok = 0;
        int idx = (int)wb_lv_tointeger(args[1], &ok);
        const char *s = wet_listbox_item(w, idx);
        return s ? str(s) : wb_lv_nil();
    }
    (void)ctx;
    return wb_lv_nil();
}

static wb_lv_value proxy_newindex_fn(wb_lv *L, wb_lv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 3) return wb_lv_nil();
    WetWidget *w = proxy_widget(L, args[0].as.tab);
    if (!w) return wb_lv_nil();
    const char *key = wb_lv_tostring(args[1]);
    if (!key) return wb_lv_nil();

    if (strcmp(key, "text") == 0) {
        const char *s = (args[2].t == WB_LV_STR) ? wb_lv_tostring(args[2]) : NULL;
        switch (w->cls->id) {
        case WET_CLASS_LABEL:   wet_label_set_text(w, s); break;
        case WET_CLASS_BUTTON:  wet_button_set_text(w, s); break;
        case WET_CLASS_TEXTBOX: wet_textbox_set_text(w, s); break;
        case WET_CLASS_CHECKBOX: {
            WetCheckBox *c = (WetCheckBox *)w->payload;
            free(c->text); c->text = strdup(s ? s : ""); break;
        }
        case WET_CLASS_GROUPBOX: {
            WetGroupBox *g = (WetGroupBox *)w->payload;
            free(g->text); g->text = strdup(s ? s : ""); break;
        }
        default: break;
        }
        return wb_lv_nil();
    }
    if (strcmp(key, "checked") == 0)  { wet_checkbox_set_checked(w, wb_lv_truthy(args[2])); return wb_lv_nil(); }
    if (strcmp(key, "value") == 0) {
        int ok = 0; int v = (int)wb_lv_tointeger(args[2], &ok);
        if (w->cls->id == WET_CLASS_PROGRESS) wet_progress_set_value(w, v);
        if (w->cls->id == WET_CLASS_SLIDER)   wet_slider_set_value(w, v);
        return wb_lv_nil();
    }
    if (strcmp(key, "maximum") == 0) { int ok=0; wet_progress_set_maximum(w,(int)wb_lv_tointeger(args[2],&ok)); return wb_lv_nil(); }
    if (strcmp(key, "minimum") == 0) { int ok=0; wet_slider_set_minimum(w,(int)wb_lv_tointeger(args[2],&ok)); return wb_lv_nil(); }
    if (strcmp(key, "selected") == 0){ int ok=0; wet_listbox_set_select(w,(int)wb_lv_tointeger(args[2],&ok)); return wb_lv_nil(); }
    if (strcmp(key, "password") == 0){ wet_textbox_set_password(w, wb_lv_truthy(args[2])); return wb_lv_nil(); }
    if (strcmp(key, "visible") == 0) { wet_widget_set_visible(w, wb_lv_truthy(args[2])); return wb_lv_nil(); }
    if (strcmp(key, "enabled") == 0) { wet_widget_set_enabled(w, wb_lv_truthy(args[2])); return wb_lv_nil(); }
    if (strcmp(key, "spacing") == 0) { int ok=0; wet_stack_set_spacing(w,(int)wb_lv_tointeger(args[2],&ok)); return wb_lv_nil(); }
    if (strcmp(key, "columns") == 0) { int ok=0; wet_grid_set_columns(w,(int)wb_lv_tointeger(args[2],&ok)); return wb_lv_nil(); }
    if (strcmp(key, "rows") == 0)    { int ok=0; wet_grid_set_rows(w,(int)wb_lv_tointeger(args[2],&ok)); return wb_lv_nil(); }
    return wb_lv_nil();
}

/* ------------------------------------------------------------ constructors */

/* wet.label{...}, wet.stack{...} and friends: stamp the class marker onto the
 * script's own table and hand it back. */
static wb_lv_value wet_ctor(wb_lv *L, wb_lv_value *args, int n, void *ud) {
    const char *cls = (const char *)ud;
    if (n < 1 || args[0].t != WB_LV_TABLE) {
        wb_lv_set_global(L, "__wet_err", str("element constructor expects a table"));
        return wb_lv_nil();
    }
    wb_lv_table_set(L, args[0].as.tab, k(L, "__wet"), str(cls));
    return args[0];
}

/* --------------------------------------------------------------- wet.applet */

static wb_lv_value wet_applet_fn(wb_lv *L, wb_lv_value *args, int n, void *ud) {
    WetLuaCtx *ctx = (WetLuaCtx *)ud;
    if (n < 1 || args[0].t != WB_LV_TABLE) {
        wb_lv_set_global(L, "__wet_err", str("wet.applet expects a table"));
        return wb_lv_nil();
    }
    wb_lv_table *t = args[0].as.tab;

    /* Dimensions are optional; the host window decides otherwise. */
    int ok = 0;
    wb_lv_value wv = wb_lv_table_get(L, t, k(L, "width"));
    wb_lv_value hv = wb_lv_table_get(L, t, k(L, "height"));
    int w = (wv.t == WB_LV_INT) ? (int)wv.as.i : ctx->app->width;
    int h = (hv.t == WB_LV_INT) ? (int)hv.as.i : ctx->app->height;

    wet_app_resize(ctx->app, w, h);

    wb_lv_value content = wb_lv_table_get(L, t, k(L, "content"));
    const char *err = NULL;
    if (content.t == WB_LV_TABLE) {
        WetWidget *root = build_widget(ctx, content.as.tab, &err);
        if (!root) {
            char buf[256];
            snprintf(buf, sizeof buf, "wet: %s", err ? err : "bad content");
            wet_app_error(ctx->app, buf);
            return wb_lv_nil();
        }
        wet_app_root(ctx->app, root);
    } else if (content.t != WB_LV_NIL) {
        wet_app_error(ctx->app, "wet: 'content' must be a widget element");
        return wb_lv_nil();
    }

    wet_app_layout(ctx->app);
    wet_app_render(ctx->app);
    (void)ok;
    return wb_lv_nil();
}

/* --------------------------------------------------------------- wet.find */

static wb_lv_value wet_find_fn(wb_lv *L, wb_lv_value *args, int n, void *ud) {
    WetLuaCtx *ctx = (WetLuaCtx *)ud;
    if (n < 1) return wb_lv_nil();
    const char *id = wb_lv_tostring(args[0]);
    WetWidget *w = wet_app_find(ctx->app, id);
    if (!w) return wb_lv_nil();

    wb_lv_table *proxy = wb_lv_table_new(L, 0, 2);
    proxy_set_ptr(L, proxy, w);
    wb_lv_setmetatable(L, proxy, ctx->proxy_meta);
    wb_lv_value out; out.t = WB_LV_TABLE; out.as.tab = proxy;
    return out;
}

/* ---------------------------------------------------------- wet.messagebox */

static wb_lv_value wet_messagebox_fn(wb_lv *L, wb_lv_value *args, int n, void *ud) {
    WetLuaCtx *ctx = (WetLuaCtx *)ud;
    const char *text = (n >= 1) ? wb_lv_tostring(args[0]) : "";
    const char *title = (n >= 2) ? wb_lv_tostring(args[1]) : "Message";
    char buf[512];
    snprintf(buf, sizeof buf, "%s\n\n%s", title ? title : "Message", text ? text : "");
    wet_app_error(ctx->app, buf);
    (void)L;
    return wb_lv_nil();
}

/* --------------------------------------------------------------- register */

void wet_lua_register(WetLua *wl) {
    if (!wl || !wl->ctx) return;
    WetLuaCtx *ctx = wl->ctx;
    g_ctx = ctx;

    wb_lv *L = ctx->L;

    /* The proxy metatable: __index / __newindex are C functions. */
    ctx->proxy_meta = wb_lv_table_new(L, 0, 2);
    wb_lv_value gi; gi.t = WB_LV_CFUNCTION; gi.as.cf.f = proxy_index_fn;   gi.as.cf.ud = ctx;
    wb_lv_value si; si.t = WB_LV_CFUNCTION; si.as.cf.f = proxy_newindex_fn; si.as.cf.ud = ctx;
    wb_lv_table_set(L, ctx->proxy_meta, k(L, "__index"), gi);
    wb_lv_table_set(L, ctx->proxy_meta, k(L, "__newindex"), si);

    /* `wet` is a real table, so `wet.applet{...}` is a field access on it --
     * registering a global literally named "wet.applet" would leave `wet`
     * nil and the script dead on line 1.  Keys are interned because the
     * interpreter hashes table keys by string identity. */
    wb_lv_table *wet = wb_lv_table_new(L, 0, 20);
    if (!wet) return;

#define WET_KEY(name) wb_lv_str(wb_lv_intern(L, (name), strlen(name)))
#define WET_FN(name, fn, udv) \
    { wb_lv_value v; v.t = WB_LV_CFUNCTION; v.as.cf.f = (fn); v.as.cf.ud = (udv); \
      wb_lv_table_set(L, wet, WET_KEY(name), v); }

    static const char *ctors[] = {
        "label", "button", "textbox", "checkbox", "listbox", "progress",
        "slider", "separator", "groupbox", "image", "stack", "dock",
        "canvas", "grid", NULL
    };
    for (int i = 0; ctors[i]; i++)
        WET_FN(ctors[i], wet_ctor, (void *)ctors[i]);

    WET_FN("applet",     wet_applet_fn, ctx);
    WET_FN("find",       wet_find_fn,   ctx);
    WET_FN("messagebox", wet_messagebox_fn, ctx);
#undef WET_KEY
#undef WET_FN

    wb_lv_value wv; wv.t = WB_LV_TABLE; wv.as.tab = wet;
    wb_lv_set_global(L, "wet", wv);

    /* Bare names too, so a script can `local find = wet.find` or call the
     * constructor unqualified -- both read naturally. */
    wb_lv_register_global(L, "applet",     wet_applet_fn, ctx);
    wb_lv_register_global(L, "find",       wet_find_fn,   ctx);
    wb_lv_register_global(L, "messagebox", wet_messagebox_fn, ctx);
    for (int i = 0; ctors[i]; i++)
        wb_lv_register_global(L, ctors[i], wet_ctor, (void *)ctors[i]);
}

/* --------------------------------------------------------------- lifecycle */

WetLua *wet_lua_new(WetApp *app, char *err, size_t err_cap) {
    if (!app) return NULL;
    WetLuaCtx *ctx = (WetLuaCtx *)calloc(1, sizeof *ctx);
    if (!ctx) return NULL;

    ctx->L = wb_lv_new();
    if (!ctx->L) { free(ctx); if (err) snprintf(err, err_cap, "lua state failed"); return NULL; }
    /* wb_lv_new() already opened the libraries; calling wb_lv_openlibs()
     * again re-initialises the state and leaves the intern table empty. */

    ctx->app = app;
    app->L      = ctx->L;

    WetLua *wl = (WetLua *)calloc(1, sizeof *wl);
    if (!wl) { wet_lua_free((WetLua *)ctx); return NULL; }
    wl->ctx = ctx;
    /* The app holds the PUBLIC handle: wet_lua_dispatch and wet_lua_run both
     * take WetLua*, and the internal ctx is wl->ctx.  Storing ctx here would
     * make the dispatcher read ctx->L as if it were a handle. */
    app->lua_ctx = wl;
    app->lua_dispatch = (void (*)(void *, WetWidget *, const WetEvent *))wet_lua_dispatch;

    wet_lua_register(wl);
    return wl;
}

void wet_lua_free(WetLua *wl) {
    if (!wl) return;
    WetLuaCtx *ctx = wl->ctx;
    if (!ctx) { free(wl); return; }
    if (ctx->app) {
        ctx->app->lua_ctx = NULL;
        ctx->app->lua_dispatch = NULL;
        ctx->app->L = NULL;
    }
    free(ctx->handlers);
    if (ctx->L) wb_lv_free(ctx->L);
    free(ctx);
    free(wl);
}

WetApp *wet_lua_app(WetLua *wl) {
    return (wl && wl->ctx) ? wl->ctx->app : NULL;
}

struct wb_lv *wet_lua_state(WetLua *wl) {
    return (wl && wl->ctx) ? wl->ctx->L : NULL;
}

/* Run a .wet script.  The script must call wet.applet{...}. */
const char *wet_lua_run(WetLua *wl, const char *src, const char *chunkname) {
    if (!wl) return "no lua state";
    WetLuaCtx *ctx = wl->ctx;
    const char *e = wb_lv_run(ctx->L, src, chunkname ? chunkname : "wet");
    if (e) return e;
    wb_lv_value er = wb_lv_get_global(ctx->L, "__wet_err");
    if (er.t == WB_LV_STR) {
        static char buf[512];
        snprintf(buf, sizeof buf, "%s", wb_lv_tostring(er));
        wb_lv_set_global(ctx->L, "__wet_err", wb_lv_nil());
        return buf;
    }
    return NULL;
}

/* Called by the framework when a control fires an event. */
void wet_lua_dispatch(WetLua *wl, WetWidget *w, const void *ev) {
    const WetEvent *evp = (const WetEvent *)ev;
    if (!wl || !wl->ctx || !w || !evp) return;
    call_handlers(wl->ctx, w, evp);
}
