/*
 * wet_lua.h -- the Lua binding for the .wet framework.
 *
 * One WetLua owns one WetApp and one Lua state.  A .wet applet is a Lua script
 * that calls wet.applet{...}; the host builds the widget tree from the tables
 * the script passes.  Event handlers are Lua functions called through
 * wb_lv_pcall, so a raising handler is reported, never fatal.
 */
#ifndef WET_LUA_H
#define WET_LUA_H

#include "wet.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WetLua WetLua;

/* Create the state and register wet.* in it.  Returns NULL and fills err on
 * failure. */
WetLua *wet_lua_new(WetApp *app, char *err, size_t err_cap);
void    wet_lua_free(WetLua *wl);

WetApp *wet_lua_app(WetLua *wl);

/* Run a .wet script.  Returns NULL on success or a static error string. */
const char *wet_lua_run(WetLua *wl, const char *src, const char *chunkname);

/* Fire an event at the script's handlers.  Called by the framework; safe to
 * call when no Lua state exists.  `ev` is the framework's internal event and
 * is only meaningful to the binding. */
void wet_lua_dispatch(WetLua *wl, WetWidget *w, const void *ev);

/* The app's Lua state, or NULL.  Used by wet_app_load. */
void *wet_app_lua(WetApp *app);

/* The raw interpreter state, for tests that must read a global back. */
struct wb_lv;
struct wb_lv *wet_lua_state(WetLua *wl);

#ifdef __cplusplus
}
#endif

#endif /* WET_LUA_H */
