/*
 * wubu_lua_compat.c -- the one place that papers over Wua's non-Windows gap.
 *
 * wb_lv_openlibs() calls wb_lv_register_coro(), which lives in Wua's
 * wb_lv_coro.c.  That file only builds on Windows (fibers), so on this host
 * the symbol is missing and any Lua consumer fails to link.
 *
 * This weak definition supplies it.  Once upstream fixes the non-Windows
 * path, the strong definition wins and this one is ignored -- no edit needed
 * here.  Every Lua host in the tree links this file; it is the single owner
 * of the workaround, so there is no second copy to drift.
 */
#include "wb_lv.h"

extern void wb_lv_error(wb_lv *L, const char *fmt, ...);   /* wb_lv_internal.h */

static wb_lv_value coro_unsupported(wb_lv *L, wb_lv_value *args, int n, void *ud) {
    (void)args; (void)n; (void)ud;
    wb_lv_error(L, "coroutines are not supported on this platform");
    return wb_lv_nil();
}

__attribute__((weak)) void wb_lv_register_coro(wb_lv *L) {
    wb_lv_table *t = wb_lv_table_new(L, 0, 6);
    if (!t) return;
#define CO(name) { \
        wb_lv_value v; v.t = WB_LV_CFUNCTION; v.as.cf.f = coro_unsupported; \
        v.as.cf.ud = NULL; \
        wb_lv_table_set(L, t, wb_lv_str(name), v); }
    CO("create"); CO("resume"); CO("yield"); CO("wrap"); CO("status"); CO("running");
#undef CO
    wb_lv_value tv; tv.t = WB_LV_TABLE; tv.as.tab = t;
    wb_lv_set_global(L, "coroutine", tv);
}
