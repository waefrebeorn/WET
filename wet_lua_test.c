/*
 * wet_lua_test.c -- behaviour gates for the .wet Lua binding.
 *
 * Builds a real applet from a real script and then reads the tree back, fires
 * events at it, and verifies the script's handlers ran.  This is the contract
 * the framework exists for: a person writes a .wet file and it behaves.
 */
#include "wet_internal.h"
#include "wet_lua.h"

#include "wb_lv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* VBE stubs so the framework links headless.  Weak, so the real ones win when
 * this is built into the kernel. */
__attribute__((weak)) void vbe_clear(unsigned c){(void)c;}
__attribute__((weak)) void vbe_fill_rect(int x,int y,int w,int h,unsigned c){(void)x;(void)y;(void)w;(void)h;(void)c;}
__attribute__((weak)) void vbe_rect(int x,int y,int w,int h,unsigned c){(void)x;(void)y;(void)w;(void)h;(void)c;}
__attribute__((weak)) void vbe_hline(int a,int b,int y,unsigned c){(void)a;(void)b;(void)y;(void)c;}
__attribute__((weak)) void vbe_draw_text(int x,int y,const char*s,unsigned c,int sc){(void)x;(void)y;(void)s;(void)c;(void)sc;}
__attribute__((weak)) int  vbe_text_width(const char*s,int sc){(void)s;(void)sc;return 0;}

/* A render backend that counts, so a script-driven render is observable. */
static int g_fills, g_texts;
static void cnt_clear(void *ctx, WetColor c){(void)ctx;(void)c; g_fills=0; g_texts=0;}
static void cnt_fill(void *ctx,int x,int y,int w,int h,WetColor c){(void)ctx;(void)x;(void)y;(void)w;(void)h;(void)c; g_fills++;}
static void cnt_rect(void *ctx,int x,int y,int w,int h,WetColor c){(void)ctx;(void)x;(void)y;(void)w;(void)h;(void)c;}
static void cnt_hline(void *ctx,int a,int b,int y,WetColor c){(void)ctx;(void)a;(void)b;(void)y;(void)c;}
static void cnt_text(void *ctx,int x,int y,const char*s,WetColor c){(void)ctx;(void)x;(void)y;(void)s;(void)c; g_texts++;}
static int  cnt_tw(void *ctx,const char*s){(void)ctx; int n=0; for(const unsigned char*p=(const unsigned char*)(s?s:"");*p;p++) if((*p&0xC0)!=0x80)n++; return n*6;}
static int  cnt_th(void *ctx){(void)ctx; return 8;}
static WetRender g_cnt = { NULL, cnt_clear, cnt_fill, cnt_rect, cnt_hline,
                           cnt_text, cnt_tw, cnt_th };

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

/* Read a field off a script table.  wb_lv_get_global takes a FLAT name, so
 * "wet.clicks" is not a field lookup -- it names a global that does not exist. */
static wb_lv_value lua_field(WetLua *wl, const char *table, const char *field) {
    wb_lv *L = wet_lua_state(wl);
    wb_lv_value t = wb_lv_get_global(L, table);
    if (t.t != WB_LV_TABLE) return wb_lv_nil();
    return wb_lv_table_get(L, t.as.tab,
                           wb_lv_str(wb_lv_intern(L, field, strlen(field))));
}

static int lua_int(WetLua *wl, const char *table, const char *field) {
    int ok = 0;
    int v = (int)wb_lv_tointeger(lua_field(wl, table, field), &ok);
    return ok ? v : -999999;
}

static const char *lua_str(WetLua *wl, const char *table, const char *field) {
    return wb_lv_tostring(lua_field(wl, table, field));
}

static const char *HELLO =
    "wet.applet {\n"
    "  title = 'Hello',\n"
    "  width = 320, height = 200,\n"
    "  content = wet.stack {\n"
    "    spacing = 6,\n"
    "    children = {\n"
    "      wet.label { id = 'greet', text = 'Name:' },\n"
    "      wet.textbox { id = 'name' },\n"
    "      wet.button { id = 'go', text = 'Say' }\n"
    "    }\n"
    "  }\n"
    "}\n";

static const char *EVENTS =
    "wet.clicks = 0\n"
    "wet.applet {\n"
    "  content = wet.stack {\n"
    "    children = {\n"
    "      wet.button { id = 'b', text = 'Go',\n"
    "        onclick = function(self) wet.clicks = wet.clicks + 1 end },\n"
    "      wet.checkbox { id = 'c', text = 'OK',\n"
    "        onchange = function(self, ev) wet.checked = ev.value end }\n"
    "    }\n"
    "  }\n"
    "}\n";

static const char *PROXY =
    "wet.applet {\n"
    "  content = wet.stack {\n"
    "    children = {\n"
    "      wet.label { id = 'l', text = 'Before' },\n"
    "      wet.button { id = 'b', text = 'Go' }\n"
    "    }\n"
    "  }\n"
    "}\n";

/* `wet.widget` is not a constructor, so the script itself fails to run. */
static const char *BAD_CTOR =
    "wet.applet { content = wet.widget { text = 'x' } }\n";

/* `content` that is not a table is rejected by the builder, not the parser. */
static const char *BAD_CONTENT =
    "wet.applet { content = 5 }\n";

static WetLua *new_app(WetApp **app_out, int w, int h) {
    WetApp *app = wet_app_new(w, h);
    app->render = &g_cnt;
    WetLua *wl = wet_lua_new(app, NULL, 0);
    if (!wl) { wet_app_free(app); return NULL; }
    *app_out = app;
    return wl;
}

static void test_build_from_script(void) {
    WetApp *app; WetLua *wl = new_app(&app, 320, 200);
    CHECK(wl != NULL, "lua state created");
    if (!wl) return;

    const char *err = wet_lua_run(wl, HELLO, "hello.wet");
    CHECK(err == NULL, err ? err : "script ran");

    WetWidget *root = app->root;
    CHECK(root != NULL, "applet built a root");
    CHECK(root->cls->id == WET_CLASS_STACK, "root is the stack");
    CHECK(wet_widget_nchildren(root) == 3, "stack has three children");
    CHECK(app->width == 320 && app->height == 200, "script dimensions applied");

    WetWidget *l = wet_app_find(app, "greet");
    WetWidget *t = wet_app_find(app, "name");
    WetWidget *b = wet_app_find(app, "go");
    CHECK(l && t && b, "all three ids resolve");
    CHECK(strcmp(wet_label_text(l), "Name:") == 0, "label text applied");
    CHECK(strcmp(wet_button_text(b), "Say") == 0, "button text applied");
    CHECK(t->cls->id == WET_CLASS_TEXTBOX, "textbox class resolved");

    wet_lua_free(wl);
    wet_app_free(app);
}

static void test_script_renders(void) {
    WetApp *app; WetLua *wl = new_app(&app, 320, 200);
    if (!wl) { CHECK(0, "lua state created"); return; }
    wet_lua_run(wl, HELLO, "hello.wet");
    wet_app_render(app);
    CHECK(g_texts > 0, "script-driven render issued text");
    wet_lua_free(wl);
    wet_app_free(app);
}

static void test_click_fires_script_handler(void) {
    WetApp *app; WetLua *wl = new_app(&app, 320, 200);
    if (!wl) { CHECK(0, "lua state created"); return; }
    wet_lua_run(wl, EVENTS, "events.wet");

    WetWidget *b = wet_app_find(app, "b");
    CHECK(b != NULL, "button found");
    if (b) {
        wet_app_pointer(app, WET_EVENT_MOUSEDOWN, b->rect.x + 2, b->rect.y + 2, 1);
        wet_app_pointer(app, WET_EVENT_MOUSEUP,   b->rect.x + 2, b->rect.y + 2, 1);
    }
    CHECK(lua_int(wl, "wet", "clicks") == 1, "onclick ran exactly once");
    wet_lua_free(wl);
    wet_app_free(app);
}

static void test_checkbox_handler_gets_value(void) {
    WetApp *app; WetLua *wl = new_app(&app, 320, 200);
    if (!wl) { CHECK(0, "lua state created"); return; }
    wet_lua_run(wl, EVENTS, "events.wet");

    WetWidget *c = wet_app_find(app, "c");
    CHECK(c != NULL, "checkbox found");
    if (c) {
        wet_app_pointer(app, WET_EVENT_MOUSEDOWN, c->rect.x + 2, c->rect.y + 2, 1);
        wet_app_pointer(app, WET_EVENT_MOUSEUP,   c->rect.x + 2, c->rect.y + 2, 1);
    }
    CHECK(wet_checkbox_checked(c), "checkbox toggled");
    CHECK(lua_int(wl, "wet", "checked") == 1, "onchange received value=1");
    wet_lua_free(wl);
    wet_app_free(app);
}

static void test_proxy_read_and_write(void) {
    WetApp *app; WetLua *wl = new_app(&app, 320, 200);
    if (!wl) { CHECK(0, "lua state created"); return; }
    wet_lua_run(wl, PROXY, "proxy.wet");

    /* Read through the proxy: run a snippet that touches the widget. */
    const char *rd =
        "local p = wet.find('l')\n"
        "if p then wet.seen = p.text else wet.seen = 'MISSING' end\n";
    const char *e1 = wet_lua_run(wl, rd, "read");
    CHECK(e1 == NULL, e1 ? e1 : "read ran");
    CHECK(strcmp(lua_str(wl, "wet", "seen"), "Before") == 0, "proxy read the label text");

    const char *wr =
        "wet.find('l').text = 'After'\n";
    const char *e2 = wet_lua_run(wl, wr, "write");
    CHECK(e2 == NULL, e2 ? e2 : "write ran");
    WetWidget *l = wet_app_find(app, "l");
    CHECK(strcmp(wet_label_text(l), "After") == 0, "proxy write changed the widget");

    const char *mi = "wet.missing = wet.find('nope')\n";
    wet_lua_run(wl, mi, "missing");
    CHECK(lua_field(wl, "wet", "missing").t == WB_LV_NIL, "unknown id yields nil");

    wet_lua_free(wl);
    wet_app_free(app);
}

static void test_unknown_constructor_is_isolated(void) {
    WetApp *app; WetLua *wl = new_app(&app, 320, 200);
    if (!wl) { CHECK(0, "lua state created"); return; }
    const char *err = wet_lua_run(wl, BAD_CTOR, "bad.wet");
    CHECK(err != NULL, "unknown constructor is reported");
    CHECK(app->root == NULL, "no partial tree is left installed");
    CHECK(wet_lua_state(wl) != NULL, "state survives a bad script");
    wet_lua_free(wl);
    wet_app_free(app);
}

static void test_non_widget_content_is_rejected(void) {
    WetApp *app; WetLua *wl = new_app(&app, 320, 200);
    if (!wl) { CHECK(0, "lua state created"); return; }
    const char *err = wet_lua_run(wl, BAD_CONTENT, "badcontent.wet");
    CHECK(err == NULL, "script parses");
    const char *le = wet_app_last_error(app);
    CHECK(le != NULL, "builder reported an error");
    CHECK(strstr(le, "content") != NULL, "error names the bad content");
    CHECK(app->root == NULL, "no partial tree is left installed");
    wet_lua_free(wl);
    wet_app_free(app);
}

static void test_nested_panels(void) {
    const char *src =
        "wet.applet {\n"
        "  content = wet.dock {\n"
        "    children = {\n"
        "      wet.label { id = 'top', text = 'Bar', dock = 2 },\n"
        "      wet.stack { id = 'body', children = {\n"
        "        wet.label { text = 'One' },\n"
        "        wet.label { text = 'Two' } } }\n"
        "    }\n"
        "  }\n"
        "}\n";
    WetApp *app; WetLua *wl = new_app(&app, 320, 200);
    if (!wl) { CHECK(0, "lua state created"); return; }
    const char *err = wet_lua_run(wl, src, "dock.wet");
    CHECK(err == NULL, err ? err : "dock script ran");

    WetWidget *top = wet_app_find(app, "top");
    WetWidget *body = wet_app_find(app, "body");
    CHECK(top && body, "both docked elements resolve");
    CHECK(top->rect.y == 0, "docked top at the top");
    CHECK(body->rect.y >= top->rect.h, "body below the docked bar");
    CHECK(wet_widget_nchildren(body) == 2, "nested stack has two labels");
    wet_lua_free(wl);
    wet_app_free(app);
}

static void test_handler_error_is_isolated(void) {
    const char *src =
        "wet.applet {\n"
        "  content = wet.stack {\n"
        "    children = {\n"
        "      wet.button { id = 'b', text = 'Go',\n"
        "        onclick = function(self) error('boom') end }\n"
        "    }\n"
        "  }\n"
        "}\n";
    WetApp *app; WetLua *wl = new_app(&app, 320, 200);
    if (!wl) { CHECK(0, "lua state created"); return; }
    wet_lua_run(wl, src, "boom.wet");
    WetWidget *b = wet_app_find(app, "b");
    if (b) {
        wet_app_pointer(app, WET_EVENT_MOUSEDOWN, b->rect.x + 2, b->rect.y + 2, 1);
        wet_app_pointer(app, WET_EVENT_MOUSEUP,   b->rect.x + 2, b->rect.y + 2, 1);
    }
    const char *le = wet_app_last_error(app);
    CHECK(le != NULL, "raising handler was reported");
    CHECK(strstr(le, "click") != NULL, "report names the raising handler");
    /* The raised message itself.  wb_lv_pcall used to stash an empty string
     * here, so this read "raised: " with nothing after the colon. */
    CHECK(strstr(le, "boom") != NULL, "report carries the raised message");
    /* The app is still alive and usable. */
    CHECK(app->root != NULL, "tree survives a raising handler");
    wet_lua_free(wl);
    wet_app_free(app);
}

static void test_find_returns_live_widget(void) {
    const char *src =
        "wet.applet {\n"
        "  content = wet.stack {\n"
        "    children = { wet.listbox { id = 'l' } }\n"
        "  }\n"
        "}\n";
    WetApp *app; WetLua *wl = new_app(&app, 320, 200);
    if (!wl) { CHECK(0, "lua state created"); return; }
    wet_lua_run(wl, src, "list.wet");
    WetWidget *l = wet_app_find(app, "l");
    wet_listbox_add(l, "alpha");
    wet_listbox_add(l, "beta");

    const char *rd = "wet.n = wet.find('l').count\n";
    const char *e = wet_lua_run(wl, rd, "count");
    CHECK(e == NULL, e ? e : "count ran");
    CHECK(lua_int(wl, "wet", "n") == 2, "proxy read the live listbox count");
    wet_lua_free(wl);
    wet_app_free(app);
}

static void test_theme_from_script(void) {
    WetApp *app; WetLua *wl = new_app(&app, 320, 200);
    CHECK(wl != NULL, "lua state created");
    if (!wl) return;

    const char *err = wet_lua_run(wl,
        "local names = wet.themes()\n"
        "wet.picked = names[1]\n"
        "wet.active = wet.theme('win98')\n"
        "wet.bad = wet.theme('nope')\n"
        "wet.err = __wet_err\n", "theme.wet");
    /* The bad call is reported twice by design: the script can read __wet_err
     * itself, and wet_lua_run consumes it and returns it as the run error. */
    CHECK(err != NULL && strstr(err, "unknown theme") != NULL,
          "the bad theme is the run error");
    const char *picked = lua_str(wl, "wet", "picked");
    const char *active = lua_str(wl, "wet", "active");
    const char *e = lua_str(wl, "wet", "err");
    CHECK(picked && strcmp(picked, "light") == 0,
          "wet.themes lists the default first");
    CHECK(active && strcmp(active, "win98") == 0,
          "wet.theme returns the name now active");
    CHECK(e && strstr(e, "unknown theme") != NULL,
          "an unknown theme sets __wet_err");
    CHECK(strcmp(wet_theme_name(), "win98") == 0,
          "a rejected theme does not switch the palette");

    wet_lua_free(wl);
    wet_app_free(app);
    wet_theme_set(NULL);   /* the theme is global; leave it as we found it */
}

int main(void) {
    printf("=== wet lua binding ===\n");
    test_build_from_script();
    test_script_renders();
    test_click_fires_script_handler();
    test_checkbox_handler_gets_value();
    test_proxy_read_and_write();
    test_unknown_constructor_is_isolated();
    test_non_widget_content_is_rejected();
    test_nested_panels();
    test_handler_error_is_isolated();
    test_find_returns_live_widget();
    test_theme_from_script();
    printf("=== wet_lua: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
