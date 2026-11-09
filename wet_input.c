/*
 * wet_input.c -- hit-testing and event routing.
 *
 * Input enters at the root and walks DOWN the tree to the deepest visible
 * child under the pointer.  If that child does not consume the event it
 * bubbles back UP through wet_dispatch(), so a panel can react to a click that
 * landed on its padding -- the .NET routed-event model.
 *
 * Keyboard events go to the focused widget, then bubble the same way.
 */
#include "wet_internal.h"

#include <stdlib.h>
#include <string.h>

/* Deepest visible widget under (x,y), or NULL. */
static WetWidget *hit_test(WetWidget *w, int x, int y) {
    if (!w || !w->visible) return NULL;
    if (!wet_rect_contains(w->rect, x, y)) return NULL;

    /* Children paint after (on top of) the parent, so test them in reverse
     * declaration order -- the last child is the topmost. */
    for (int i = w->nchildren - 1; i >= 0; i--) {
        WetWidget *hit = hit_test(w->children[i], x, y);
        if (hit) return hit;
    }
    return w;
}

/* The widget that took the last MOUSEDOWN, so a release over it can raise
 * the CLICK a control listens for.  .NET raises Click on MouseUp inside the
 * same control; a drag out and back must not count. */
static WetWidget *g_pressed;

/* Route a pointer event.  Returns 1 if something consumed it. */
int wet_app_pointer(WetApp *app, WetEventKind kind, int x, int y, int button) {
    if (!app || !app->root) return 0;
    WetWidget *hit = hit_test(app->root, x, y);

    /* A release with no press (or a press that began elsewhere) is not a
     * click, but it still needs a target to land on. */
    if (kind == WET_EVENT_MOUSEUP && hit != g_pressed) {
        g_pressed = NULL;
        if (!hit) return 0;
    }
    if (!hit) return 0;

    WetEvent ev; memset(&ev, 0, sizeof ev);
    ev.widget = hit; ev.button = button;
    ev.x = x; ev.y = y;
    ev.kind = (kind == WET_EVENT_MOUSEUP && g_pressed == hit) ? WET_EVENT_CLICK : kind;
    if (kind == WET_EVENT_MOUSEDOWN) g_pressed = hit;

    /* A click on a control that can take focus moves the focus to it. */
    if (kind == WET_EVENT_MOUSEDOWN && hit->cls->input) {
        bool focusable = (hit->cls->id == WET_CLASS_TEXTBOX ||
                          hit->cls->id == WET_CLASS_LISTBOX ||
                          hit->cls->id == WET_CLASS_BUTTON ||
                          hit->cls->id == WET_CLASS_CHECKBOX ||
                          hit->cls->id == WET_CLASS_SLIDER);
        if (focusable) wet_widget_set_focus(app, hit);
    }

    /* Walk down to the target so each widget sees local coordinates. */
    WetWidget *chain[64];
    int n = 0;
    for (WetWidget *w = hit; w && n < 64; w = w->parent) chain[n++] = w;

    for (int i = 0; i < n; i++) {
        WetWidget *w = chain[i];
        if (!w->visible || !w->enabled) continue;
        if (w->cls->input && w->cls->input(w, &ev)) return 1;
    }

    /* Not consumed by a control: bubble the routed event. */
    WetEvent bev = ev;
    wet_dispatch(app, hit, &bev);
    return 1;
}

/* Route a keyboard event to the focused widget. */
int wet_app_key(WetApp *app, WetEventKind kind, int key, uint32_t mods) {
    if (!app || !app->focus) return 0;
    WetEvent ev; memset(&ev, 0, sizeof ev);
    ev.kind = kind; ev.widget = app->focus; ev.target = app->focus;
    ev.key = key; ev.mods = mods;

    WetWidget *chain[64];
    int n = 0;
    for (WetWidget *w = app->focus; w && n < 64; w = w->parent) chain[n++] = w;

    for (int i = 0; i < n; i++) {
        WetWidget *w = chain[i];
        if (!w->visible || !w->enabled) continue;
        if (w->cls->input && w->cls->input(w, &ev)) return 1;
    }
    WetEvent bev = ev;
    wet_dispatch(app, app->focus, &bev);
    return 1;
}
