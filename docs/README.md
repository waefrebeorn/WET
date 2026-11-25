# WET -- the applet runtime

Shell surfaces are `.WET` applets, not hand-drawn C. This repo is the runtime that
parses and runs them.

## Build

`make` -- consumed by WuBuOS as a submodule; the parent also compiles these sources
directly in `mk/tests.mk`.

## Verify

**There is no `test` target yet.** That is a declared gap, not a clean bill: a real
gate needs a headless applet host that loads a fixture `.wet` file, renders it, and
asserts the resulting pixels and hit-test rects. The parent's `test_wet_applet` loads
a shipped applet and asserts the tree shape, which is weaker.

## Docs

Parent: `docs/compendium/04-roadmap/win11-desktop-standard.md` is the contract for
which surface is `.WET` and which is C.
