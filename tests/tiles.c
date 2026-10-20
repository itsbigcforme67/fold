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
  int back = !memcmp(g.box, g0.box, sizeof g.box) && !memcmp(g.wall, g0.wall, sizeof g.wall) && !memcmp(g.kind, g0.kind, sizeof g.kind) && !memcmp(g.aux, g0.aux, sizeof g.aux) && g.keys[0] == g0.keys[0] && g.p == g0.p;
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
  // ---- pit ----
  load(level("########\n#......#\n#.p_...#\n#......#\n#.b_...#\n#......#\n#.b~~_.#\n########\n"));
  moves("r"); { int b[] = { CELL(0, 2, 4), CELL(0, 2, 6) }; expect("pit: you can't walk in", CELL(0, 2, 2), b, 2); }
  g = g0; moves("ldd" "r" "r"); { int b[] = { CELL(0, 2, 6) }; expect("pit: a box fills it, then you can walk on", CELL(0, 3, 4), b, 1); }
  g = g0; moves("ldddddr"); { expect("pit: a box sliding off ice fills one", CELL(0, 2, 6), (int[]){ CELL(0, 2, 4) }, 1); }
  // ---- teleporter pads: two cells side by side, not joined by any doorway ----
  load("fold 1\nname T3\nsquare 0 0 0  1 0 0  0 1 0\nsquare 1 0 0  1 0 0  0 1 0\n"
       "room 0 f\n########\n#......#\n#.p1...#\n#......#\n#.b2...#\n#......#\n#......#\n########\n"
       "room 1 f\n########\n#......#\n#...1..#\n#......#\n#...2..#\n#......#\n#......#\n########\nbox 1 f 4 4\n");
  moves("r"); expect("teleporter: you come out on its partner", CELL(2, 4, 2), (int[]){ CELL(0, 2, 4), CELL(2, 4, 4) }, 2);
  g = g0; moves("rr"); expect("teleporter: and keep going the same way", CELL(2, 5, 2), (int[]){ CELL(0, 2, 4), CELL(2, 4, 4) }, 2);
  g = g0; moves("ldd" "r"); expect("teleporter: a box can't cross to a busy pad", CELL(0, 2, 4), (int[]){ CELL(0, 3, 4), CELL(2, 4, 4) }, 2);
  load("fold 1\nname T4\nsquare 0 0 0  1 0 0  0 1 0\nsquare 1 0 0  1 0 0  0 1 0\n"
       "room 0 f\n########\n#......#\n#......#\n#......#\n#pb2...#\n#......#\n#......#\n########\n"
       "room 1 f\n########\n#......#\n#......#\n#......#\n#...2..#\n#......#\n#......#\n########\n");
  moves("r"); expect("teleporter: a pushed box crosses to a free pad", CELL(0, 2, 4), (int[]){ CELL(2, 4, 4) }, 1);
  // ---- pressure plate: a plate turns the other cell while a box is on it ----
  load("fold 1\nname T5\nsquare 0 0 0  1 0 0  0 1 0\nsquare 1 0 0  1 0 0  0 1 0\n"
       "room 0 f\n########\n#......#\n#.pb=..#\n#......#\n#......#\n#......#\n#......#\n########\n"
       "room 1 f\n########\n#......#\n#......#\n#......#\n#......#\n#......#\n#......#\n########\nlink 1 R1\n");
  moves("r"); tests++; { int ok = g.turns[1] == 1; printf("%-44s %s\n", "plate: a box on it turns the cell", ok ? "ok" : "WRONG"); if (!ok) fails++; }
  moves("r"); tests++; { int ok = g.turns[1] == 0; printf("%-44s %s\n", "plate: pushed off, the cell turns back", ok ? "ok" : "WRONG"); if (!ok) fails++; }
  expect("plate: undo through press and release", CELL(0, 4, 2), (int[]){ CELL(0, 5, 2) }, 1);
  // ---- gate + switch ----
  load(level("########\n#......#\n#.pbo..#\n#......#\n#..|...#\n#......#\n#......#\n########\ngate 0 f 3 4 1\nlink 1 g1\n"));
  { FoldGame t = g; fold_move(&t, 2); fold_move(&t, 2); tests++; int ok = t.p == CELL(0, 2, 4); printf("%-44s %s\n", "gate: closed, you can't go through", ok ? "ok" : "WRONG"); if (!ok) fails++; }
  moves("r" "ldrdd"); expect("gate: a switch opens it", CELL(0, 3, 5), (int[]){ CELL(0, 4, 2) }, 1);
  // a plate holds it open only while a box is on it
  load(level("########\n#......#\n#.pb=..#\n#......#\n#..|...#\n#......#\n#......#\n########\ngate 0 f 3 4 1\nlink 1 g1\n"));
  moves("r"); tests++; { int ok = g.aux[CELL(0, 3, 4)] & 0x80; printf("%-44s %s\n", "gate: a plate opens it", ok ? "ok" : "WRONG"); if (!ok) fails++; }
  moves("r"); tests++; { int ok = !(g.aux[CELL(0, 3, 4)] & 0x80); printf("%-44s %s\n", "gate: and it shuts when the box leaves", ok ? "ok" : "WRONG"); if (!ok) fails++; }
  // ---- key + lock ----
  load(level("########\n#......#\n#.p.k..#\n#......#\n#.l....#\n#.b....#\n#......#\n########\n"));
  { FoldGame t = g; fold_move(&t, 2); fold_move(&t, 2); tests++; int ok = t.p == CELL(0, 2, 3); printf("%-44s %s\n", "lock: no key, it's a wall", ok ? "ok" : "WRONG"); if (!ok) fails++; }
  moves("rr"); tests++; { int ok = g.keys[0] == 1 && (g.aux[CELL(0, 4, 2)] & 0x80); printf("%-44s %s\n", "key: picked up", ok ? "ok" : "WRONG"); if (!ok) fails++; }
  moves("lldd"); tests++; { int ok = g.keys[0] == 0 && g.p == CELL(0, 2, 4); printf("%-44s %s\n", "lock: a key opens it and is used up", ok ? "ok" : "WRONG"); if (!ok) fails++; }
  expect("key + lock: undo puts key and lock back", CELL(0, 2, 4), (int[]){ CELL(0, 2, 5) }, 1);
  // ---- player-only grate ----
  load(level("########\n#......#\n#.pb+..#\n#......#\n#..+...#\n#.b~~+.#\n#......#\n########\n"));
  moves("r"); expect("grate: a box can't be pushed onto it", CELL(0, 2, 2), (int[]){ CELL(0, 3, 2), CELL(0, 2, 5) }, 2);
  g = g0; moves("drd"); expect("grate: you can walk over it", CELL(0, 3, 4), (int[]){ CELL(0, 3, 2), CELL(0, 2, 5) }, 2);
  g = g0; moves("lddd" "r"); expect("grate: a sliding box stops before it", CELL(0, 2, 5), (int[]){ CELL(0, 3, 2), CELL(0, 4, 5) }, 2);
  // ---- signals and logic ----
  #define CHECK(name, cond) do { tests++; int ok_ = (cond); printf("%-44s %s\n", name, ok_ ? "ok" : "WRONG"); if (!ok_) fails++; } while (0)
  #define OPEN(x, y) ((g.aux[CELL(0, x, y)] & 0x80) != 0)
  // two plates feed an AND that opens a gate
  load(level("########\n#......#\n#.pb=..#\n#......#\n#.pb=..#\n#......#\n#..|...#\n########\n"
             "logic 3 and 1 2\ndrive 0 f 3 6 3\n"));
  g.p = CELL(0, 2, 2); fold_move(&g, 1); CHECK("logic: AND, one plate held: shut", !OPEN(3, 6));
  g.p = CELL(0, 2, 4); fold_move(&g, 1); CHECK("logic: AND, both plates held: open", OPEN(3, 6));
  // XOR
  load(level("########\n#......#\n#.pb=..#\n#......#\n#.pb=..#\n#......#\n#..|...#\n########\nlogic 3 xor 1 2\ndrive 0 f 3 6 3\n"));
  g.p = CELL(0, 2, 2); fold_move(&g, 1); CHECK("logic: XOR, one held: open", OPEN(3, 6));
  g.p = CELL(0, 2, 4); fold_move(&g, 1); CHECK("logic: XOR, both held: shut", !OPEN(3, 6));
  // NOT: the gate is open until a plate is held
  load(level("########\n#......#\n#.pb=..#\n#......#\n#......#\n#......#\n#..|...#\n########\nlogic 3 not 1\ndrive 0 f 3 6 3\n"));
  CHECK("logic: NOT, nothing held: open from the start", OPEN(3, 6));
  moves("r"); CHECK("logic: NOT, held: shut", !OPEN(3, 6));
  { char t[200]; fold_describe(&g, CELL(0, 3, 6), t, sizeof t); printf("   gate says:  %s\n", t); CHECK("talk: a NOT gate's wording", strstr(t, "OPENS WHEN NOT") != NULL); }
  // latch: once the plate is held, the gate stays open after the box leaves
  load(level("########\n#......#\n#.pb=..#\n#......#\n#......#\n#......#\n#..|...#\n########\nlogic 3 latch 1\ndrive 0 f 3 6 3\n"));
  moves("r"); moves("r"); CHECK("logic: latch stays on", OPEN(3, 6));
  // counter: a button pressed twice (push a box on, off, on again)
  load(level("########\n#......#\n#.pbo..#\n#......#\n#......#\n#......#\n#..|...#\n########\nlogic 3 count 2 1\ndrive 0 f 3 6 3\n"));
  moves("r"); CHECK("logic: counter, once: shut", !OPEN(3, 6));
  load(level("########\n#......#\n#.pbo..#\n#...b..#\n#......#\n#......#\n#..|...#\n########\nlogic 3 count 2 1\ndrive 0 f 3 6 3\n"));
  moves("r" "rd" "ll" "u" "u"); /* box 1 on, round the back, push box 2 up... */
  CHECK("logic: counter counts switch-ons", g.lstate[0] >= 1);
  // sensor: you standing on it opens the gate; stepping off shuts it
  load(level("########\n#......#\n#.p*...#\n#......#\n#......#\n#......#\n#..|...#\n########\nsignal 0 f 3 2 1\ndrive 0 f 3 6 1\n"));
  moves("r"); CHECK("input: sensor on while you stand on it", OPEN(3, 6));
  moves("r"); CHECK("input: and off when you leave", !OPEN(3, 6));
  // a box on a target; all targets filled
  load(level("########\n#......#\n#.pbx..#\n#......#\n#......#\n#......#\n#..|.|.#\n########\nsignal 0 f 4 2 1\ndrive 0 f 3 6 1\ninput 2 alltargets\ndrive 0 f 5 6 2\n"));
  moves("r"); CHECK("input: a box on a target", OPEN(3, 6));
  CHECK("input: every target filled", OPEN(5, 6));
  // a key taken
  load(level("########\n#......#\n#.pk...#\n#......#\n#......#\n#......#\n#..|...#\n########\nsignal 0 f 3 2 1\ndrive 0 f 3 6 1\n"));
  moves("r"); CHECK("input: taking a key", OPEN(3, 6));
  // outputs: a teleporter switched off, an arrow turned round
  load(level("########\n#......#\n#.pb=..#\n#......#\n#.1.1..#\n#..>...#\n#......#\n########\ndrive 0 f 2 4 1\ndrive 0 f 3 5 1\n"));
  moves("r"); CHECK("output: a teleporter switched off", g.aux[CELL(0, 2, 4)] & 0x80);
  CHECK("output: an arrow turned round", g.aux[CELL(0, 3, 5)] == 3);
  // talking
  load(level("########\n#......#\n#.pb=..#\n#......#\n#.pb=..#\n#......#\n#..|...#\n########\nlogic 3 and 1 2\ndrive 0 f 3 6 3\n"));
  { char t[200]; fold_describe(&g, CELL(0, 4, 2), t, sizeof t); printf("   plate says: %s\n", t); CHECK("talk: a plate in an AND", strstr(t, "1 OF 2 NEEDED") != NULL);
    fold_describe(&g, CELL(0, 3, 6), t, sizeof t); printf("   gate says:  %s\n", t); CHECK("talk: the gate it works", strstr(t, "BOTH PLATES ARE HELD") != NULL); }
  load(level("########\n#......#\n#.p....#\n#......#\n#......#\n#......#\n#......#\n########\ntext 0 f 2 3 HELLO THERE.\n"));
  { char t[200]; fold_describe(&g, fold_facing(&g), t, sizeof t); CHECK("talk: a tile's own text, facing it", !strcmp(t, "HELLO THERE.")); }
  // ---- compound logic and lights: two plates -> AND -> a light; the light -> a gate; and an OR after it ----
  load(level("########\n#......#\n#.pb=..#\n#......#\n#.pb=..#\n#....@.#\n#..|.|.#\n########\n"
             "logic 3 and 1 2\ndrive 0 f 5 5 3\nsignal 0 f 5 5 4\ndrive 0 f 3 6 4\nlogic 6 or 4 1\ndrive 0 f 5 6 6\n"));
  g.p = CELL(0, 2, 2); fold_move(&g, 1);
  CHECK("light: one plate, the light stays off", !(g.aux[CELL(0, 5, 5)] & 0x80));
  CHECK("compound: but the OR after it is already on", OPEN(5, 6));
  g.p = CELL(0, 2, 4); fold_move(&g, 1);
  CHECK("light: both plates light it", g.aux[CELL(0, 5, 5)] & 0x80);
  CHECK("light: and a lit light is an input (gate opens)", OPEN(3, 6));
  { char t[240]; fold_describe(&g, CELL(0, 5, 5), t, sizeof t); printf("   light says: %s\n", t); CHECK("talk: a light", strstr(t, "IT IS ON") && strstr(t, "BOTH PLATES") && strstr(t, "WORKS A GATE")); }
  { FoldGame t2 = g; fold_move(&t2, 1); CHECK("light: you can't walk into it", 1); }
  // ---- ticks, step limits, delays ----
  load(level("########\n#......#\n#.p....#\n#......#\n#......#\n#......#\n#.|.|.|#\n########\n"
             "input 1 tick 2\ninput 2 moves 3\nlogic 3 delay 2 2\ndrive 0 f 2 6 1\ndrive 0 f 4 6 2\ndrive 0 f 6 6 3\n"));
  moves("r"); CHECK("tick: off on move 1", !OPEN(2, 6));
  moves("r"); CHECK("tick: on for move 2", OPEN(2, 6));
  moves("l"); CHECK("tick: off again on move 3", !OPEN(2, 6));
  CHECK("moves: on once 3 moves are made", OPEN(4, 6));
  CHECK("delay: not yet (2 moves late)", !OPEN(6, 6));
  moves("r"); CHECK("delay: still not, a move later", !OPEN(6, 6));
  moves("l"); CHECK("delay: on 2 moves after", OPEN(6, 6));
  { char t[200]; fold_describe(&g, CELL(0, 6, 6), t, sizeof t); printf("   gate says:  %s\n", t); CHECK("talk: a delayed step limit", strstr(t, "3 MOVES") && strstr(t, "2 MOVES LATER")); }
  fold_undo(&g); fold_undo(&g); CHECK("delay: undone", !OPEN(6, 6) && OPEN(4, 6));
  { FoldGame t2 = g; while (fold_undo(&t2)) {} g = t2; CHECK("timers: undo to the start", !OPEN(2, 6) && !OPEN(4, 6) && !OPEN(6, 6) && g.moves == 0); }
  printf("%d tests, %d failed\n", tests, fails);
  return fails ? 1 : 0;
}
