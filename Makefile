# .WET Framework — Makefile
#
# The framework is plain C11 with one optional dependency: Wua (slerm-Lua),
# linked in as a submodule under externals/wua.  Everything else is in-tree.
#
#   make            build the test binaries
#   make test       build and run both suites
#   make check      alias for test
#
# A WuBuOS build does not use this file: it compiles the framework straight
# into the kernel and links the real vbe.c instead of hosted/vbe_shim.c.

CC      ?= cc
CFLAGS  ?= -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra
LDLIBS  ?= -lm

WUA     := externals/wua/src
WUA_SRCS := $(WUA)/wb_lv_arena.c $(WUA)/wb_lv_eval.c $(WUA)/wb_lv_host.c \
            $(WUA)/wb_lv_lex.c $(WUA)/wb_lv_parse.c $(WUA)/wb_lv_value.c \
            $(WUA)/wb_lv_stdlib.c $(WUA)/wb_lv_string.c

FRAMEWORK := wet_widget.c wet_layout.c wet_render.c wet_controls.c \
             wet_app.c wet_input.c wet_backend_vbe.c

TESTS := build/test_wet build/test_wet_lua

.PHONY: all test check clean
all: $(TESTS)

build:
	mkdir -p build

# Framework only -- no interpreter, no VBE.
build/test_wet: wet_selftest.c $(FRAMEWORK) hosted/vbe_shim.c | build
	$(CC) $(CFLAGS) -I. -Ihosted $^ $(LDLIBS) -o $@

# Framework + Lua binding, against Wua.
build/test_wet_lua: wet_lua_test.c wet_lua.c wubu_lua_compat.c $(FRAMEWORK) \
                   hosted/vbe_shim.c $(WUA_SRCS) | build
	$(CC) $(CFLAGS) -I. -Ihosted -I$(WUA) $^ $(LDLIBS) -o $@

test check: $(TESTS)
	./build/test_wet
	./build/test_wet_lua

clean:
	rm -rf build
