# .WET Framework

**WuBu's .net** — a declarative, scriptable applet stack for WuBuOS.

A `.wet` applet is a Lua script that builds a widget tree and hands it to the
framework.  The framework runs a real **measure/arrange** layout pass (the WPF
model, not a pile of hand-placed rects), renders through the VBE so a `.wet`
applet composites with the same Win98 shell as a C app, and calls back into
Lua when the user touches something.

C11 substrate, Lua for the surface.  Heavy and stable in C; creative and
fast-moving in Lua.

---

## The model

| .WET | .NET |
|---|---|
| `WetApp` | `Application` — owns the tree, the focus, the render backend |
| `WetWidget` | `FrameworkElement` — id, desired size, final rect, handlers |
| `stack` / `dock` / `canvas` / `grid` | `Panel` — measures and arranges children |
| `label` / `button` / `textbox` / … | `Control` |
| `onclick`, `onchange`, … | routed events, bubbling to the parent panel |

Layout is two-pass: a panel asks each child *how big do you want to be*, then
says *here is what you get*.  A control never positions itself, so the same
script reflows correctly at any window size.

## A .wet applet

```lua
wet.clicks = 0

wet.applet {
  title  = "Hello",
  width  = 320,
  height = 200,

  content = wet.stack {
    spacing = 6,
    children = {
      wet.label  { id = "greet", text = "Name:" },
      wet.textbox{ id = "name" },
      wet.button { id = "go", text = "Say",
                   onclick = function(self)
                     wet.messagebox("Hello, " .. (wet.find("name").text or ""))
                   end },
      wet.checkbox { id = "c", text = "OK",
                     onchange = function(self, ev) wet.checked = ev.value end },
    },
  },
}
```

Children are an explicit `children = { ... }` array.  Numeric keys
(`wet.stack { wet.label{...}, wet.label{...} }`) also work, but only while no
property precedes them — see the Wua table-literal note below.

`wet.find(id)` returns a **proxy** whose metatable reads and writes the live
widget, so `wet.find("name").text = "x"` is a real property assignment.

## Elements

**Controls** — `label`, `button`, `textbox`, `checkbox`, `listbox`,
`progress`, `slider`, `separator`, `groupbox`, `image`.

**Panels** — `stack` (`spacing`, `orient`, `expand`), `dock` (per-child
`dock` = 1..5 for left/top/right/bottom/fill), `canvas` (per-child `left`,
`top`), `grid` (`columns`, `rows`, per-child `col`, `row`).

**Events** — `onclick`, `ondblclick`, `onchange`, `ontextchanged`,
`onselect`, `onfocus`, `onblur`, `onkeydown`, `onkeyup`, `onmousedown`,
`onmouseup`, `onmousemove`, `onpaint`.

**Globals** — `wet.applet{...}`, `wet.find(id)`, `wet.messagebox(text, title)`.
Constructors are also registered bare (`label{...}`) for scripts that prefer it.

## Building

```bash
git submodule update --init --recursive   # fetches Wua
make test
```

`make test` builds and runs two suites: `test_wet` (framework geometry, layout,
input routing, rendering — 51 checks) and `test_wet_lua` (script-driven build,
proxies, handlers, error isolation — 38 checks).

The framework is plain C11 and has **one** dependency: Wua, for the Lua
binding.  A pure-C applet links without it — the interpreter is an optional
bridge, reached through a function pointer the app fills in.

## Layout in WuBuOS

WuBuOS consumes this repository as a submodule at `src/gui/wet` and links the
real `vbe.c`.  `hosted/vbe_shim.c` exists only so this repository's own tests
run without a kernel; it is never compiled into the OS.

---

## Interface contract with Wua

`.WET` needs these from Wua.  All are already present except where noted.

**Required, working**
- `wb_lv_new` / `wb_lv_free`, `wb_lv_run`, `wb_lv_get_global` /
  `wb_lv_set_global`, `wb_lv_register_global`
- `wb_lv_table_new` / `set` / `get` / `len`, `wb_lv_intern`
- `wb_lv_call`, `wb_lv_pcall`
- `wb_lv_str` / `strl` / `integer` / `number` / `bool` / `nil`,
  `wb_lv_tostring`, `wb_lv_tonumber`, `wb_lv_tointeger`, `wb_lv_truthy`
- metatables: `wb_lv_getmetatable`, `wb_lv_setmetatable`, `wb_lv_has_meta`
- the full Lua 5.4 language surface, including string methods (`s:upper()`)

**Required, not yet correct**
- `wb_lv_pcall` reports the caught message through the global
  `__lv_last_error`.  It is currently set to an **empty string**; the Lua-level
  `pcall` returns the right text.  `.WET` reads it to report a raising
  handler, so every handler error currently reads as "raised: ".
- `wb_lv_coro.c` does not compile on non-Windows hosts (the `#else` branches
  reference `co`, which is declared inside the `#ifdef`).  Until it builds,
  `wubu_lua_compat.c` supplies a weak `wb_lv_register_coro` so the rest links.

**Workaround in place (no action needed)**
- A table literal with a hash-part key *before* its array items loses the
  array part (`{spacing=6, 'a','b'}` has length 0).  `.WET` therefore uses an
  explicit `children = { ... }` array, which is unaffected.

See `WET.md` in the WuBuOS repository for the integration notes.
