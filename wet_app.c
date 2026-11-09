/*
 * wet_app.c -- the applet application.
 *
 * WetApp is the .NET Application object: it owns the widget tree, the input
 * focus, the render backend and (when the applet is script-driven) the Lua
 * state.  wet_app_layout() runs measure then arrange over the whole tree;
 * wet_app_render() clears and draws it.
 */
#include "wet_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void wet_controls_register(void);
void wet_layout_register(void);

static void wet_register_builtins(void) {
    static int done;
    if (done) return;
    wet_controls_register();
    wet_layout_register();
    done = 1;
}

WetApp *wet_app_new(int width, int height) {
    wet_register_builtins();

    WetApp *app = (WetApp *)calloc(1, sizeof *app);
    if (!app) return NULL;
    app->width  = width  > 0 ? width  : 640;
    app->height = height > 0 ? height : 480;
    app->render = wet_render_vbe();
    return app;
}

void wet_app_free(WetApp *app) {
    if (!app) return;
    wet_widget_free(app->root);
    free(app->index);
    free(app);
}

void wet_app_root(WetApp *app, WetWidget *w) {
    if (!app) return;
    app->root = w;
    if (w) w->parent = NULL;
}

void wet_app_resize(WetApp *app, int width, int height) {
    if (!app) return;
    app->width  = width  > 0 ? width  : app->width;
    app->height = height > 0 ? height : app->height;
    wet_app_layout(app);
}

void wet_app_layout(WetApp *app) {
    if (!app || !app->root) return;
    WetSize avail = { app->width, app->height };
    wet_widget_measure(app->root, avail);
    WetSize want = app->root->desired;
    WetRect r = (WetRect){ app->origin_x, app->origin_y,
                           want.w < app->width  ? app->width  : want.w,
                           want.h < app->height ? app->height : want.h };
    wet_widget_arrange(app->root, r);
}

/* Position the applet's (0,0) on screen.  A windowed applet calls this with
 * its content rect before rendering, so the same tree draws fullscreen or in
 * a panel without knowing which it is. */
void wet_app_set_origin(WetApp *app, int x, int y) {
    if (!app) return;
    app->origin_x = x;
    app->origin_y = y;
    wet_app_layout(app);
}

void wet_app_render(WetApp *app) {
    if (!app || !app->render || !app->root) return;
    const WetTheme *t = wet_theme_get();
    if (app->render->clear) app->render->clear(app->render->ctx, t->window);
    wet_widget_render(app->root, app->render);
}

WetWidget *wet_app_find(const WetApp *app, const char *id) {
    return wet_app_lookup((WetApp *)app, id);
}

void *wet_app_lua(WetApp *app) { return app ? app->L : NULL; }

/* The last error the applet reported.  A .wet handler that raises, or a
 * builder that rejects a table, lands here rather than taking the shell down. */
static char g_err[512];

const char *wet_app_error(WetApp *app, const char *msg) {
    (void)app;
    if (!msg) return NULL;
    snprintf(g_err, sizeof g_err, "%s", msg);
    return g_err;
}

const char *wet_app_last_error(const WetApp *app) {
    (void)app;
    return g_err[0] ? g_err : NULL;
}

/* Load and build a .wet applet from Lua source.  The binding must already be
 * registered (wet_lua_new); this only runs the script.  The reference is weak
 * so a pure-C applet links without the interpreter. */
typedef const char *(*wet_lua_run_fn)(void *wl, const char *src, const char *chunkname);
__attribute__((weak)) const char *wet_lua_run(void *wl, const char *src, const char *chunkname);

const char *wet_app_load(WetApp *app, const char *src, const char *chunkname) {
    if (!app || !src) return "no app or source";
    if (!app->lua_ctx) return "no lua state";
    if (!wet_lua_run) return "no lua binding";
    return wet_lua_run(app->lua_ctx, src, chunkname);
}

void *wet_app_data(WetApp *app) { return app ? app->userdata : NULL; }
