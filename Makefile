# FOLD: the C core and its tests (PC). `make test` replays every level; `make parity` checks the C core against the
# JavaScript one move by move (needs node and the microgames collection beside this repo).
CC ?= gcc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter
NODE ?= $(HOME)/.local/node/bin/node

build/levels: tests/levels.c core/fold.c core/fold.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tests/levels.c core/fold.c

build/tiles: tests/tiles.c core/fold.c core/fold.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tests/tiles.c core/fold.c

test: build/levels build/tiles
	build/levels levels/*.fold
	build/tiles

parity: build/levels
	$(NODE) tools/trace_js.mjs > build/trace_js.txt
	build/levels -t levels/*.fold > build/trace_c.txt
	@if cmp -s build/trace_js.txt build/trace_c.txt; then echo "parity: C and JavaScript agree on every move ($$(grep -vc '^#' build/trace_c.txt) moves)"; \
	else echo "parity: DIFFERENT"; diff build/trace_js.txt build/trace_c.txt | head -20; exit 1; fi

APP = app/app.c app/gfx.c app/vmx.c core/fold.c
APPH = app/app.h app/gfx.h app/vmx.h core/fold.h

build/fold_headless: app/main_headless.c $(APP) $(APPH)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ app/main_headless.c $(APP) -lm

build/fold: app/main_sdl.c $(APP) $(APPH)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ app/main_sdl.c $(APP) $$(sdl2-config --cflags --libs) -lm

app: build/fold build/fold_headless

.PHONY: test parity app
