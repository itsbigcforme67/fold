// Replays every level's solution through the C core: solved at the end, and undoing every move gets back exactly to
// the start. With -t prints a trace (one line per move) for the parity test against the JavaScript core.
//   build/levels [-t] levels/*.fold
#include "../core/fold.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FoldLevel lv;
static FoldGame g, start;

static unsigned sum(const uint8_t *a, int n) { unsigned s = 0; for (int c = 0; c < n; c++) if (a[c]) s += (unsigned)(c + 1) * a[c]; return s; }
static void trace(const FoldGame *g) {
  int C = 2 * g->lv->nsq * FOLD_NN;
  printf("%d %d %d|", g->p, g->camf, g->camrot);
  for (int c = 0; c < C; c++) if (g->box[c]) printf("%d,", c);
  printf("|%u %u %u|", sum(g->wall, C), sum(g->target, C), sum(g->btn, C));
  for (int i = 0; i < g->lv->nsq; i++) printf("%d%d%d", g->flips[i], g->turns[i], g->moved[i]);
  printf("\n");
}
static int same(const FoldGame *a, const FoldGame *b) {
  int C = 2 * a->lv->nsq * FOLD_NN;
  return a->p == b->p && a->camf == b->camf && a->camrot == b->camrot && !memcmp(a->wall, b->wall, C) &&
         !memcmp(a->target, b->target, C) && !memcmp(a->box, b->box, C) && !memcmp(a->btn, b->btn, C) &&
         !memcmp(a->flips, b->flips, a->lv->nsq) && !memcmp(a->turns, b->turns, a->lv->nsq) &&
         !memcmp(a->moved, b->moved, a->lv->nsq) && !memcmp(&a->s.adj, &b->s.adj, sizeof a->s.adj);
}
int main(int argc, char **argv) {
  int tr = 0, bad = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-t")) { tr = 1; continue; }
    FILE *f = fopen(argv[i], "rb"); if (!f) { printf("%s: can't open\n", argv[i]); bad++; continue; }
    static char text[1 << 16]; size_t n = fread(text, 1, sizeof text - 1, f); text[n] = 0; fclose(f);
    char err[128];
    if (!fold_parse(&lv, text, err, sizeof err)) { printf("%s: %s\n", argv[i], err); bad++; continue; }
    fold_start(&g, &lv); start = g;
    if (tr) printf("# %s\n", argv[i]);
    int ok = 1, k;
    for (k = 0; k < lv.nsol && ok; k++) {
      int d = lv.sol[k] - '0';
      if (!fold_move(&g, (d - g.camrot) & 3)) { ok = 0; break; }
      if (tr) trace(&g);
    }
    int solved = ok && fold_solved(&g);
    while (fold_undo(&g)) {}
    int back = same(&g, &start);
    if (!tr) printf("%-28s %-12s %s, %d moves, undo %s\n", argv[i], lv.name, solved ? "solved" : ok ? "NOT SOLVED" : "STUCK", k, back ? "back to start" : "WRONG");
    if (!solved || !back) bad++;
  }
  return bad ? 1 : 0;
}
