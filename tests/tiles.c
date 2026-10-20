// Rules of each tile kind, on small hand-made levels: a move sequence, then where you and the boxes should be.
// Every test also undoes back to the start and checks nothing is left over.   build/tiles
#include "../core/fold.h"
#include <stdio.h>
#include <string.h>

static FoldLevel lv; static FoldGame g, g0;
static int fails, tests;
#define CELL(f, x, y) ((f) * 64 + (y) * 8 + (x))   // f = side: cell i front is 2i, back 2i + 1
static void load(const char *text) { char err[96]; if (!fold_parse(&lv, text, err, sizeof err)) { printf("parse: %s\n", err); fails++; } fold_start(&g, &lv); g0 = g; }
static void moves(const char *m) {   // u r d l: screen directions
  for (; *m; m++) { int sd = *m == 'u' ? 0 : *m == 'r' ? 1 : *m == 'd' ? 2 : 3; fold_move(&g, sd); }
}
static void expect(const char *name, int p, const int *boxes, int nb) {
  tests++;
  int ok = g.p == p, n = 0;
  for (int c = 0; c < FOLD_MAXC; c++) if (g.box[c]) { n++; int found = 0; for (int k = 0; k < nb; k++) if (boxes[k] == c) found = 1; if (!found) ok = 0; }
  if (n != nb) ok = 0;
  while (fold_undo(&g)) {}
  int back = !memcmp(g.box, g0.box, sizeof g.box) && !memcmp(g.wall, g0.wall, sizeof g.wall) && !memcmp(g.kind, g0.kind, sizeof g.kind) && g.p == g0.p;
  printf("%-44s %s%s\n", name, ok ? "ok" : "WRONG", back ? "" : "  (undo NOT back to start)");
  if (!ok || !back) fails++;
}

static const char *ONE = "fold 1\nname T\nsquare 0 0 0  1 0 0  0 1 0\nroom 0 f\n";
static char buf[2048];
static const char *level(const char *rows) { snprintf(buf, sizeof buf, "%s%s", ONE, rows); return buf; }

int main(void) {
  // ---- ice ----
  load(level("########\n#p~~~..#\n#......#\n#.b~~~.#\n#......#\n#.~b...#\n#......#\n########\n"));
  moves("r"); { int b[] = { CELL(0, 2, 3), CELL(0, 3, 5) }; expect("ice: you slide off the far end", CELL(0, 5, 1), b, 2); }
  g = g0; moves("ddr"); { int b[] = { CELL(0, 6, 3), CELL(0, 3, 5) }; expect("ice: a pushed box slides to the end", CELL(0, 2, 3), b, 2); }
  g = g0; moves("ddddl" "r"); { int b[] = { CELL(0, 2, 3), CELL(0, 3, 5) }; expect("ice: your slide stops at a box", CELL(0, 2, 5), b, 2); }
  // ice across a doorway: two cells side by side (a flat join); you slide out of one and on into the other
  load("fold 1\nname T2\nsquare 0 0 0  1 0 0  0 1 0\nsquare 1 0 0  1 0 0  0 1 0\n"
       "room 0 f\n########\n#......#\n#......#\n#p.~~~~~\n#......#\n#......#\n#......#\n########\n"
       "room 1 f\n########\n#......#\n#......#\n~~~....#\n#......#\n#......#\n#......#\n########\n");
  moves("rr"); { expect("ice: a slide carries on through a doorway", CELL(2, 3, 3), NULL, 0); }
  // ---- one-way floor ----
  load(level("########\n#......#\n#..p...#\n#..>...#\n#.b>...#\n#......#\n#......#\n########\n"));
  moves("d"); { int b[] = { CELL(0, 2, 4) }; expect("one-way: can't step in against its arrow", CELL(0, 3, 2), b, 1); }
  g = g0; moves("llddr"); { int b[] = { CELL(0, 3, 4) }; expect("one-way: a box goes onto it the arrow's way", CELL(0, 2, 4), b, 1); }
  load(level("########\n#...p..#\n#...b..#\n#...>..#\n#......#\n#......#\n#......#\n########\n"));
  moves("d"); { int b[] = { CELL(0, 4, 2) }; expect("one-way: but can't be pushed in sideways", CELL(0, 4, 1), b, 1); }
  // a turned cell turns its arrows: one cell, a button that turns it; the arrow then points down
  load(level("########\n#......#\n#.pbo..#\n#..>...#\n#......#\n#......#\n#......#\n########\nlink 1 r0\n"));
  moves("r"); tests++; { int ok = g.kind[CELL(0, 4, 3)] == FK_ONEWAY && g.aux[CELL(0, 4, 3)] == 2; printf("%-44s %s\n", "one-way: turns with its cell", ok ? "ok" : "WRONG"); if (!ok) fails++; }
  load(level("########\n#......#\n#.pbo..#\n#..>...#\n#......#\n#......#\n#......#\n########\nlink 1 f0\n"));
  moves("r"); tests++; { int ok = g.kind[CELL(1, 4, 3)] == FK_ONEWAY && g.aux[CELL(1, 4, 3)] == 3; printf("%-44s %s\n", "one-way: mirrors when its cell flips", ok ? "ok" : "WRONG"); if (!ok) fails++; }
  printf("%d tests, %d failed\n", tests, fails);
  return fails ? 1 : 0;
}
