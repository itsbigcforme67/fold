// FOLD core. See fold.h; the rules match the JavaScript version (ui/games/foldrooms_core.js in microgames).
#include "fold.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define N FOLD_N
#define NN FOLD_NN
static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };

// ---- small integer vectors ---------------------------------------------------------------------------------------
typedef struct { int x[3]; } V3;
static V3 v3(const int8_t *a) { V3 r = { { a[0], a[1], a[2] } }; return r; }
static V3 add(V3 a, V3 b) { V3 r = { { a.x[0] + b.x[0], a.x[1] + b.x[1], a.x[2] + b.x[2] } }; return r; }
static V3 sub(V3 a, V3 b) { V3 r = { { a.x[0] - b.x[0], a.x[1] - b.x[1], a.x[2] - b.x[2] } }; return r; }
static V3 neg(V3 a) { V3 r = { { -a.x[0], -a.x[1], -a.x[2] } }; return r; }
static V3 mul(V3 a, int k) { V3 r = { { a.x[0] * k, a.x[1] * k, a.x[2] * k } }; return r; }
static int dot(V3 a, V3 b) { return a.x[0] * b.x[0] + a.x[1] * b.x[1] + a.x[2] * b.x[2]; }
static V3 cross(V3 a, V3 b) {
  V3 r = { { a.x[1] * b.x[2] - a.x[2] * b.x[1], a.x[2] * b.x[0] - a.x[0] * b.x[2], a.x[0] * b.x[1] - a.x[1] * b.x[0] } };
  return r;
}
static int eq(V3 a, V3 b) { return a.x[0] == b.x[0] && a.x[1] == b.x[1] && a.x[2] == b.x[2]; }

// a face's frame: front = (o, u, v, n), back = mirror (o + u, -u, v, -n); corners TL TR BR BL
typedef struct { V3 o, u, v, n; } Frame;
static Frame frame(const FoldSquare *q, int back) {
  Frame f = { v3(q->o), v3(q->u), v3(q->v), v3(q->n) };
  if (back) { f.o = add(f.o, f.u); f.u = neg(f.u); f.n = neg(f.n); }
  return f;
}
static V3 corner(const Frame *f, int k) {
  switch (k & 3) { case 0: return f->o; case 1: return add(f->o, f->u); case 2: return add(add(f->o, f->u), f->v); }
  return add(f->o, f->v);
}

// ---- the surface -------------------------------------------------------------------------------------------------
// Off face f's edge e: turn about the edge from f's own half-plane towards its normal side and land on the first
// square met, on its side facing back. Directions are grid unit vectors, so the turn is a quarter count: 1, 2 or 3
// quarters (4 = your own back = a rim = wall).
void fold_surface(FoldSurface *s, const FoldSquare *sq, int nsq) {
  s->nsq = nsq;
  memcpy(s->sq, sq, sizeof(FoldSquare) * nsq);
  for (int f = 0; f < 2 * nsq; f++) {
    Frame F = frame(&sq[f >> 1], f & 1);
    for (int e = 0; e < 4; e++) {
      V3 p = corner(&F, e), q = corner(&F, e + 1);
      V3 dA = sub(add(mul(F.o, 2), add(F.u, F.v)), add(p, q));      // 2 x (centre - edge middle): inward, unit
      int best = 4, bestSq = -1;
      for (int j = 0; j < nsq; j++) {
        if (j == f >> 1) continue;
        Frame G = frame(&sq[j], 0);
        int k;                                                        // does square j have this edge?
        for (k = 0; k < 4; k++) {
          V3 a = corner(&G, k), b = corner(&G, k + 1);
          if ((eq(a, p) && eq(b, q)) || (eq(a, q) && eq(b, p))) break;
        }
        if (k == 4) continue;
        V3 d = sub(add(mul(G.o, 2), add(G.u, G.v)), add(p, q));
        int dn = dot(d, F.n), dd = dot(d, dA), turn = dn == 1 ? 1 : dd == -1 ? 2 : dn == -1 ? 3 : 4;
        if (turn < best) { best = turn; bestSq = j; }
      }
      s->adj[f][e].f = -1; s->adj[f][e].e = 0;
      if (bestSq < 0) continue;                                       // a rim
      V3 t = best == 1 ? neg(dA) : best == 2 ? neg(F.n) : dA;         // the way the turn is going when it lands
      int g = 2 * bestSq + (dot(v3(sq[bestSq].n), t) < 0 ? 0 : 1);
      Frame G = frame(&sq[bestSq], g & 1);
      for (int k = 0; k < 4; k++)
        if (eq(corner(&G, k), q) && eq(corner(&G, k + 1), p)) { s->adj[f][e].f = g; s->adj[f][e].e = k; }
    }
  }
}
int fold_step(const FoldSurface *s, int c, int d, int *nd) {
  int f = c / NN, x = c % N, y = (c / N) % N, nx = x + DX[d], ny = y + DY[d];
  if (nx >= 0 && ny >= 0 && nx < N && ny < N) { *nd = d; return f * NN + ny * N + nx; }
  FoldJoin a = s->adj[f][d];
  if (a.f < 0) return -1;
  int p = d == 0 ? x : d == 1 ? y : d == 2 ? N - 1 - x : N - 1 - y, p2 = N - 1 - p, x2, y2;
  switch (a.e) { case 0: x2 = p2; y2 = 0; break; case 1: x2 = N - 1; y2 = p2; break;
                 case 2: x2 = N - 1 - p2; y2 = N - 1; break; default: x2 = 0; y2 = N - 1 - p2; }
  *nd = (a.e + 2) & 3;
  return a.f * NN + y2 * N + x2;
}

// ---- cell geometry: where tiles go when a cell flips or turns, where a shift / hinge puts it ----------------------------
static int twin(int c) { int f = c / NN, x = c % N, y = (c / N) % N; return (f ^ 1) * NN + y * N + (N - 1 - x); }
static int rot_cell(int c) {                 // a quarter turn clockwise as seen from the front
  int f = c / NN, x = c % N, y = (c / N) % N;
  return f * NN + ((f & 1) ? (N - 1 - x) * N + y : x * N + (N - 1 - y));
}
void fold_to_screen(int fx, int fy, int rot, int *sx, int *sy) {
  switch (rot & 3) { case 0: *sx = fx; *sy = fy; break; case 1: *sx = fy; *sy = N - 1 - fx; break;
                     case 2: *sx = N - 1 - fx; *sy = N - 1 - fy; break; default: *sx = N - 1 - fy; *sy = fx; }
}
static V3 rot90(V3 w, V3 a, int sgn) { return add(mul(cross(a, w), sgn), mul(a, dot(a, w))); }   // about unit a
FoldSquare fold_geo_pose(FoldSquare q, const FoldAct *a) {
  V3 o = v3(q.o), u = v3(q.u), v = v3(q.v), n = v3(q.n);
  if (a->kind == 's') {
    V3 d = a->spec[0] == 'u' ? neg(v) : a->spec[0] == 'd' ? v : a->spec[0] == 'l' ? neg(u) : a->spec[0] == 'r' ? u :
           a->spec[0] == 'o' ? n : neg(n);
    o = add(o, d);
  } else {
    Frame F = { o, u, v, n };
    int e = a->spec[0] - '0', sg = a->spec[1] == '-' ? -1 : 1;
    V3 A = corner(&F, e), ax = sub(corner(&F, e + 1), A), m2 = sub(add(mul(o, 2), add(u, v)), mul(A, 2));
    int dir = dot(rot90(m2, ax, 1), n) * sg > 0 ? 1 : -1;            // swing the middle towards sg x n
    o = add(A, rot90(sub(o, A), ax, dir)); u = rot90(u, ax, dir); v = rot90(v, ax, dir); n = rot90(n, ax, dir);
  }
  for (int i = 0; i < 3; i++) { q.o[i] = o.x[i]; q.u[i] = u.x[i]; q.v[i] = v.x[i]; q.n[i] = n.x[i]; }
  return q;
}
int fold_overlap(const FoldSquare *sq, int nsq) {   // same middle (doubled, so integer) and the same plane
  for (int i = 0; i < nsq; i++) for (int j = i + 1; j < nsq; j++) {
    V3 mi = add(mul(v3(sq[i].o), 2), add(v3(sq[i].u), v3(sq[i].v))), mj = add(mul(v3(sq[j].o), 2), add(v3(sq[j].u), v3(sq[j].v)));
    if (eq(mi, mj) && abs(dot(v3(sq[i].n), v3(sq[j].n))) == 1) return 1;
  }
  return 0;
}
static void rebuild(FoldGame *g) {
  FoldSquare sq[FOLD_MAXSQ];
  for (int i = 0; i < g->lv->nsq; i++) sq[i] = g->moved[i] ? fold_geo_pose(g->lv->sq[i], &g->lv->geoAct[i]) : g->lv->sq[i];
  fold_surface(&g->s, sq, g->lv->nsq);
}

// ---- changes, logged for undo -----------------------------------------------------------------------------------
// Every change a move makes goes through set(): the old value is logged, and undo plays the log backwards. That keeps
// undo exact whatever a move does (slides, room moves, ...).
enum { A_WALL, A_TARGET, A_BOX, A_BTN, A_KIND, A_AUX, A_DRV, A_TXT, A_FLIPS, A_TURNS, A_MOVED, A_KEYS, A_TOG, A_SIG, A_LST, A_LPREV };
static uint8_t *arr(FoldGame *g, int a) {
  switch (a) { case A_WALL: return g->wall; case A_TARGET: return g->target; case A_BOX: return g->box; case A_BTN: return g->btn;
               case A_KIND: return g->kind; case A_AUX: return g->aux; case A_DRV: return g->drv; case A_TXT: return g->txt;
               case A_FLIPS: return g->flips; case A_TURNS: return g->turns; case A_KEYS: return g->keys; case A_TOG: return g->toggled;
               case A_SIG: return g->sig; case A_LST: return g->lstate; case A_LPREV: return g->lprev; }
  return g->moved;
}
static void drop_oldest(FoldGame *g) {        // the journal or the move list is full: forget the oldest move
  uint32_t cut = g->nundo > 1 ? g->undo[1].j : g->nj;
  memmove(g->journal, g->journal + cut, sizeof(uint32_t) * (g->nj - cut)); g->nj -= cut;
  memmove(g->undo, g->undo + 1, sizeof(FoldUndo) * (g->nundo - 1)); g->nundo--;
  for (int k = 0; k < g->nundo; k++) g->undo[k].j -= cut;
}
static void set(FoldGame *g, int a, int i, int v) {
  uint8_t *p = arr(g, a);
  if (p[i] == v) return;
  while (g->nj == FOLD_JOURNAL && g->nundo > 1) drop_oldest(g);
  if (g->nj < FOLD_JOURNAL) g->journal[g->nj++] = (uint32_t)a << 24 | (uint32_t)i << 8 | p[i];
  p[i] = (uint8_t)v;
}

// ---- room moves --------------------------------------------------------------------------------------------------
// A tile carried to another spot: all its layers move; a direction it holds is turned / mirrored with it.
typedef struct { uint8_t v[8]; } TileV;   // a tile's layers, in A_WALL .. A_TXT order
static TileV tile_get(FoldGame *g, int c) { TileV t = { { g->wall[c], g->target[c], g->box[c], g->btn[c], g->kind[c], g->aux[c], g->drv[c], g->txt[c] } }; return t; }
static void tile_put(FoldGame *g, int c, TileV t) { for (int a = 0; a < 8; a++) set(g, a, c, t.v[a]); }
static void flip_square(FoldGame *g, int i) {
  for (int c = 2 * i * NN; c < (2 * i + 1) * NN; c++) {
    int t = twin(c); TileV a = tile_get(g, c), b = tile_get(g, t);
    // the other side is the mirror image: a one-way's left / right swap
    if (a.v[4] == FK_ONEWAY && a.v[5] & 1) a.v[5] ^= 2;
    if (b.v[4] == FK_ONEWAY && b.v[5] & 1) b.v[5] ^= 2;
    tile_put(g, c, b); tile_put(g, t, a);
  }
  if ((g->p / NN) >> 1 == i) g->p = twin(g->p);
  if (g->camf >> 1 == i) g->camf ^= 1;
  set(g, A_FLIPS, i, g->flips[i] ^ 1);
}
static void turn_square(FoldGame *g, int i) {
  TileV old[2 * NN]; int lo = 2 * i * NN;
  for (int c = lo; c < lo + 2 * NN; c++) old[c - lo] = tile_get(g, c);
  for (int c = lo; c < lo + 2 * NN; c++) {
    TileV t = old[c - lo];
    if (t.v[4] == FK_ONEWAY) t.v[5] = (uint8_t)((t.v[5] + ((c / NN) & 1 ? 3 : 1)) & 3);   // turns with it (the back sees it anticlockwise)
    tile_put(g, rot_cell(c), t);
  }
  if ((g->p / NN) >> 1 == i) g->p = rot_cell(g->p);
  if (g->camf >> 1 == i) {                    // your room holds still on screen: the view turns with it
    int c = g->camf * NN + 2 * N + 1, r = rot_cell(c), sx, sy;
    fold_to_screen(1, 2, g->camrot, &sx, &sy);
    for (int q = 0; q < 4; q++) { int x, y; fold_to_screen(r % N, (r / N) % N, q, &x, &y); if (x == sx && y == sy) { g->camrot = q; break; } }
  }
  set(g, A_TURNS, i, (g->turns[i] + 1) & 3);
}
static int apply_act(FoldGame *g, const FoldAct *a, int back) {   // back: the opposite move (a turn goes back)
  switch (a->kind) {
    case 'f': flip_square(g, a->sq); return FOLD_FLIPPED;
    case 'r': case 'R': for (int k = 0; k < (back ? 3 : 1); k++) turn_square(g, a->sq); return FOLD_TURNED;
    default: set(g, A_MOVED, a->sq, g->moved[a->sq] ^ 1); rebuild(g); return a->kind == 's' ? FOLD_SHIFTED : FOLD_HINGED;
  }
}

// ---- signals -----------------------------------------------------------------------------------------------------
// Inputs are read from the tiles (they ride along with their cells), logic pieces are worked out in order, and every
// signal that changed works its outputs. Outputs can move cells, which can change inputs, so it goes round again until
// nothing changes (or 8 times, against wiring that never settles).
static int alltargets(const FoldGame *g) {
  int any = 0;
  for (int c = 0; c < 2 * g->lv->nsq * NN; c++) if (g->target[c] && !g->wall[c]) { any = 1; if (!g->box[c]) return 0; }
  return any;
}
static void inputs(FoldGame *g, uint8_t *v) {
  memset(v, 0, FOLD_MAXSIG);
  for (int s = 1; s < FOLD_MAXSIG; s++) v[s] = g->toggled[s];
  for (int c = 0; c < 2 * g->lv->nsq * NN; c++) {
    int s = g->btn[c]; if (!s || g->wall[c]) continue;
    switch (g->kind[c]) {
      case FK_PLATE: v[s] |= g->box[c]; break;
      case FK_SENSOR: v[s] |= c == g->p; break;
      case FK_KEY: v[s] |= (g->aux[c] & 0x80) != 0; break;
      case FK_LIGHT: v[s] |= (g->aux[c] & 0x80) != 0; break;   // a lit light
      default: if (g->target[c]) v[s] |= g->box[c]; break;   // a plain button is in toggled[]
    }
  }
  if (g->lv->allSig) v[g->lv->allSig] |= (uint8_t)alltargets(g);
  for (int k = 0; k < g->lv->ntimer; k++) {   // timers count your moves
    const FoldTimer *T = &g->lv->timer[k]; int n = T->n ? T->n : 1;
    v[T->sig] |= (uint8_t)(T->kind == FT_TICK ? g->moves > 0 && g->moves % n == 0 : g->moves >= n);
  }
}
// once a move: each delay piece takes in its input as it stood when the last move ended (its outputs see it n moves
// later)
static void delays(FoldGame *g) {
  for (int k = 0; k < g->lv->nlogic; k++) if (g->lv->logic[k].op == FL_DELAY)
    set(g, A_LST, k, (uint8_t)(g->lstate[k] << 1 | (g->sig[g->lv->logic[k].in[0]] != 0)));
}
static void logic(FoldGame *g, uint8_t *v) {
  for (int k = 0; k < g->lv->nlogic; k++) {
    const FoldLogic *L = &g->lv->logic[k];
    int on = 0, n = 0;
    for (int i = 0; i < L->n; i++) n += v[L->in[i]] != 0;
    switch (L->op) {
      case FL_AND: on = n == L->n; break;
      case FL_OR: on = n > 0; break;
      case FL_XOR: on = n & 1; break;
      case FL_NOT: on = !v[L->in[0]]; break;
      case FL_LATCH:
        if (L->n > 1 && v[L->in[1]]) set(g, A_LST, k, 0); else if (v[L->in[0]]) set(g, A_LST, k, 1);
        on = g->lstate[k]; break;
      case FL_COUNT:
        if (v[L->in[0]] && !g->lprev[k] && g->lstate[k] < 255) set(g, A_LST, k, g->lstate[k] + 1);
        set(g, A_LPREV, k, v[L->in[0]] != 0);
        on = g->lstate[k] >= L->count; break;
      case FL_DELAY: { int n = L->count < 1 ? 1 : L->count > 8 ? 8 : L->count; on = g->lstate[k] >> (n - 1) & 1; break; }
    }
    v[L->out] |= (uint8_t)on;
  }
}
static int outputs(FoldGame *g, int s, int on) {   // signal s just switched on / off
  int r = 0;
  for (int k = 0; k < g->lv->nact[s - 1]; k++) {
    const FoldAct *a = &g->lv->act[s - 1][k];
    r |= apply_act(g, a, a->kind == 'R' && !on);  // 'R': turned while on, back while off
  }
  for (int c = 0; c < 2 * g->lv->nsq * NN; c++) if (g->drv[c] == s && !g->wall[c]) {
    if (g->kind[c] == FK_GATE || g->kind[c] == FK_TELE || g->kind[c] == FK_LIGHT) { set(g, A_AUX, c, g->aux[c] ^ 0x80); r |= FOLD_GATES; }
    else if (g->kind[c] == FK_ONEWAY) { set(g, A_AUX, c, (g->aux[c] + 2) & 3); r |= FOLD_GATES; }
  }
  return r;
}
static int evaluate(FoldGame *g) {
  int r = 0;
  for (int pass = 0; pass < 8; pass++) {
    uint8_t v[FOLD_MAXSIG]; int changed = 0;
    inputs(g, v); logic(g, v);
    for (int s = 1; s < FOLD_MAXSIG; s++) if ((v[s] != 0) != (g->sig[s] != 0)) {
      set(g, A_SIG, s, v[s] != 0); r |= outputs(g, s, v[s] != 0); changed = 1;
    }
    if (!changed) break;
  }
  return r;
}

// ---- game --------------------------------------------------------------------------------------------------------
void fold_start(FoldGame *g, const FoldLevel *lv) {
  memset(g, 0, sizeof *g);
  g->lv = lv;
  memcpy(g->wall, lv->wall, sizeof g->wall); memcpy(g->target, lv->target, sizeof g->target);
  memcpy(g->box, lv->box, sizeof g->box); memcpy(g->btn, lv->btn, sizeof g->btn);
  memcpy(g->kind, lv->kind, sizeof g->kind); memcpy(g->aux, lv->aux, sizeof g->aux);
  memcpy(g->drv, lv->drv, sizeof g->drv); memcpy(g->txt, lv->txt, sizeof g->txt);
  fold_surface(&g->s, lv->sq, lv->nsq);
  g->p = lv->player; g->camf = g->p / NN; g->camrot = 0; g->facing = 2;
  // outputs are their drawn state, switched while their signal is on: a signal already on at the start (a NOT, a box
  // starting on a target...) works its outputs once now. That isn't a move, so it can't be undone.
  evaluate(g);
  g->nj = 0;
}
// can a box (box = 1) or you (box = 0) go onto tile c, arriving with heading d?
static int enterable(const FoldGame *g, int c, int d, int box) {
  (void)box;
  if (c < 0 || g->wall[c]) return 0;
  if (g->kind[c] == FK_ONEWAY && g->aux[c] != d) return 0;   // only in the arrow's way
  if (g->kind[c] == FK_PIT) return box;                       // you can't walk in; a box drops in and fills it
  if (g->kind[c] == FK_GATE && !(g->aux[c] & 0x80)) return 0; // a closed gate (whatever stands in it can still leave)
  if (g->kind[c] == FK_LOCK) return !box && g->keys[0] > 0;   // you with a key can open it
  if (g->kind[c] == FK_GRATE) return !box;                    // you can cross a grate, a box can't
  if (g->kind[c] == FK_LIGHT) return 0;                        // a light is a post: talk to it from beside it
  return 1;
}
// a box arrives at c heading d: into a pit it drops (the pit becomes floor, the box is gone; returns -1); on ice it
// keeps sliding until the next tile won't take it. Returns where it stops.
static int box_slide(FoldGame *g, int c, int d) {
  for (int guard = 0; guard < FOLD_MAXC; guard++) {
    if (g->kind[c] == FK_PIT) { set(g, A_BOX, c, 0); set(g, A_KIND, c, FK_FLOOR); return -1; }
    if (g->kind[c] != FK_ICE) break;
    int nd, n = fold_step(&g->s, c, d, &nd);
    if (n < 0 || !enterable(g, n, nd, 1) || g->box[n] || n == g->p) break;
    set(g, A_BOX, c, 0); set(g, A_BOX, n, 1); c = n; d = nd;
  }
  return c;
}
// a teleporter's partner (the other pad with its pair number), or -1
static int partner(const FoldGame *g, int c) {   // (switched-off pads don't work)
  if (g->aux[c] & 0x80) return -1;
  for (int k = 0; k < 2 * g->lv->nsq * NN; k++) if (k != c && g->kind[k] == FK_TELE && g->aux[k] == g->aux[c] && !g->wall[k]) return k;
  return -1;
}
// you step onto tile c: a lock opens (using a key)
static void step_on(FoldGame *g, int c) {
  if (g->kind[c] == FK_LOCK) { set(g, A_KEYS, 0, g->keys[0] - 1); set(g, A_KIND, c, FK_FLOOR); }
}
int fold_move(FoldGame *g, int sd) {
  int d = (sd + g->camrot) & 3, d1, d2;
  int c1 = fold_step(&g->s, g->p, d, &d1);
  if (!enterable(g, c1, d1, 0)) return 0;
  int c2 = -1;
  if (g->box[c1]) {
    c2 = fold_step(&g->s, c1, d1, &d2);
    if (!enterable(g, c2, d2, 1) || g->box[c2]) return 0;
  }
  if (g->nundo == FOLD_MAXUNDO) drop_oldest(g);
  FoldUndo *u = &g->undo[g->nundo++];
  u->p = (int16_t)g->p; u->camf = (uint8_t)g->camf; u->camrot = (uint8_t)g->camrot; u->pushed = c2 >= 0;
  u->facing = (uint8_t)g->facing; u->j = g->nj;
  int r = FOLD_MOVED;
  if (c2 >= 0) {                              // the push: the box may slide on ice, then press what it stops on
    set(g, A_BOX, c1, 0); set(g, A_BOX, c2, 1); g->pushes++; r |= FOLD_PUSHED;
    g->p = c1;                                 // you are behind it now (so it can't slide back through you)
    c2 = box_slide(g, c2, d2);
    if (c2 >= 0 && g->kind[c2] == FK_TELE) {   // onto a pad: across to its partner if that's free
      int t = partner(g, c2);
      if (t >= 0 && !g->box[t] && t != c1) { set(g, A_BOX, c2, 0); set(g, A_BOX, t, 1); c2 = t; r |= FOLD_TELEPORTED; }
    }
  }
  int c = c1, hd = d1;                        // you: one step, then on ice keep sliding (a box stops you)
  step_on(g, c);
  for (int guard = 0; guard < FOLD_MAXC && g->kind[c] == FK_ICE; guard++) {
    int nd, n = fold_step(&g->s, c, hd, &nd);
    if (!enterable(g, n, nd, 0) || g->box[n]) break;
    c = n; hd = nd; step_on(g, c);
  }
  if (g->kind[c] == FK_TELE) {                // onto a pad: across to its partner (same heading there) if it's free
    int t = partner(g, c);
    if (t >= 0 && !g->box[t]) { c = t; r |= FOLD_TELEPORTED; }
  }
  if (g->kind[c] == FK_KEY && !(g->aux[c] & 0x80)) { set(g, A_KEYS, 0, g->keys[0] + 1); set(g, A_AUX, c, g->aux[c] | 0x80); }   // a key
  g->p = c; g->moves++; g->facing = hd;
  if (c / NN != g->camf) { g->camf = c / NN; g->camrot = (hd - sd) & 3; r |= FOLD_CROSSED; }
  // a box landing on a plain button flips its signal; then every signal is worked out and its outputs move
  if (c2 >= 0 && g->btn[c2] && g->kind[c2] == FK_FLOOR && !g->target[c2]) set(g, A_TOG, g->btn[c2], !g->toggled[g->btn[c2]]);
  delays(g);
  r |= evaluate(g);
  return r;
}
int fold_undo(FoldGame *g) {
  if (!g->nundo) return 0;
  FoldUndo *u = &g->undo[--g->nundo];
  int geo = 0;
  while (g->nj > u->j) {                      // play the move's changes backwards
    uint32_t e = g->journal[--g->nj];
    int a = (int)(e >> 24), i = (int)(e >> 8 & 0xffff);
    arr(g, a)[i] = (uint8_t)(e & 0xff);
    if (a == A_MOVED) geo = 1;
  }
  if (geo) rebuild(g);
  g->p = u->p; g->camf = u->camf; g->camrot = u->camrot; g->facing = u->facing; g->moves--; g->pushes -= u->pushed;
  return 1;
}
int fold_solved(const FoldGame *g) {
  for (int c = 0; c < 2 * g->lv->nsq * NN; c++) if (g->box[c] && !g->target[c]) return 0;
  return 1;
}

// ---- talking ----------------------------------------------------------------------------------------------------
int fold_facing(const FoldGame *g) { int nd; return fold_step(&g->s, g->p, g->facing, &nd); }
// What a tile says. The level's own text if it has one; otherwise what kind of thing it is and what it does, worked
// out from the wiring, always as "a cell" / "a gate" (never which one or where: finding that out is the game).
static void cat(char *o, int len, const char *s) { size_t l = strlen(o); if ((int)l < len - 1) { strncpy(o + l, s, (size_t)len - 1 - l); o[len - 1] = 0; } }
static const char *src_name(const FoldGame *g, int s) {   // what sends signal s
  static char tb[40];
  for (int k = 0; k < g->lv->ntimer; k++) if (g->lv->timer[k].sig == s) {
    const FoldTimer *T = &g->lv->timer[k];
    if (T->kind == FT_TICK) { if (T->n <= 1) snprintf(tb, sizeof tb, "YOU MOVE"); else snprintf(tb, sizeof tb, "EVERY %d MOVES", T->n); }
    else snprintf(tb, sizeof tb, "YOU HAVE MADE %d MOVES", T->n);
    return tb;
  }
  for (int c = 0; c < 2 * g->lv->nsq * NN; c++) if (g->btn[c] == s && !g->wall[c]) {
    switch (g->kind[c]) { case FK_PLATE: return "A PLATE IS HELD"; case FK_SENSOR: return "YOU STAND ON A SENSOR"; case FK_KEY: return "A KEY IS TAKEN";
                          case FK_LIGHT: return "A LIGHT IS ON"; }
    return g->target[c] ? "A BOX IS ON A TARGET" : "A BUTTON IS PRESSED";
  }
  if (g->lv->allSig == s) return "EVERY TARGET IS FILLED";
  return NULL;
}
static void cond(const FoldGame *g, int s, char *o, int len, int depth) {   // when signal s is on
  const char *n = src_name(g, s);
  if (n) { cat(o, len, n); return; }
  for (int k = 0; k < g->lv->nlogic; k++) if (g->lv->logic[k].out == s) {
    const FoldLogic *L = &g->lv->logic[k];
    if (depth > 1) { cat(o, len, "SOMETHING ELSE HAPPENS"); return; }
    if (L->op == FL_NOT) { cat(o, len, "NOT ("); cond(g, L->in[0], o, len, depth + 1); cat(o, len, ")"); return; }
    if (L->op == FL_LATCH) { cond(g, L->in[0], o, len, depth + 1); cat(o, len, " (ONCE IS ENOUGH)"); return; }
    if (L->op == FL_COUNT) { char b[24]; cond(g, L->in[0], o, len, depth + 1); snprintf(b, sizeof b, " %d TIMES", L->count); cat(o, len, b); return; }
    if (L->op == FL_DELAY) { char b[32]; cond(g, L->in[0], o, len, depth + 1); snprintf(b, sizeof b, L->count > 1 ? " (%d MOVES LATER)" : " (A MOVE LATER)", L->count); cat(o, len, b); return; }
    // inputs all of one kind read as one phrase: BOTH PLATES ARE HELD, EITHER BUTTON IS PRESSED...
    static const char *ONE[] = { "A PLATE IS HELD", "A BUTTON IS PRESSED", "A BOX IS ON A TARGET", "A KEY IS TAKEN", "A LIGHT IS ON" };
    static const char *MANY[] = { "PLATES ARE HELD", "BUTTONS ARE PRESSED", "BOXES ARE ON THEIR TARGETS", "KEYS ARE TAKEN", "LIGHTS ARE ON" };
    static const char *NOUN[] = { "PLATE IS HELD", "BUTTON IS PRESSED", "BOX IS ON ITS TARGET", "KEY IS TAKEN", "LIGHT IS ON" };
    const char *n0 = src_name(g, L->in[0]); int same = n0 != NULL, kind = -1;
    for (int i = 1; i < L->n; i++) { const char *ni = src_name(g, L->in[i]); if (!ni || !n0 || strcmp(ni, n0)) same = 0; }
    for (int k = 0; k < 5 && same; k++) if (!strcmp(n0, ONE[k])) kind = k;
    if (same && kind >= 0 && L->n > 1) {
      char b[64];
      if (L->op == FL_AND) { if (L->n == 2) snprintf(b, sizeof b, "BOTH %s", MANY[kind]); else snprintf(b, sizeof b, "ALL %d %s", L->n, MANY[kind]); }
      else if (L->op == FL_OR) { if (L->n == 2) snprintf(b, sizeof b, "EITHER %s", NOUN[kind]); else snprintf(b, sizeof b, "ANY OF %d: A %s", L->n, NOUN[kind]); }
      else snprintf(b, sizeof b, "JUST ONE %s", NOUN[kind]);
      cat(o, len, b);
      return;
    }
    for (int i = 0; i < L->n; i++) {
      if (i) cat(o, len, L->op == FL_AND ? " AND " : " OR ");
      cond(g, L->in[i], o, len, depth + 1);
    }
    if (L->op == FL_XOR) cat(o, len, " (BUT NOT BOTH)");
    return;
  }
  cat(o, len, "SOMETHING HAPPENS");
}
static void effect(const FoldGame *g, int s, char *o, int len) {   // what signal s does: one sentence a kind of effect
  int n = 0, moves = 0;
  for (int k = 0; k < g->lv->nact[s - 1]; k++) {
    char kd = g->lv->act[s - 1][k].kind;
    cat(o, len, moves++ ? " AND " : n++ ? ". IT " : " IT ");
    cat(o, len, kd == 'f' ? "FLIPS A CELL" : kd == 's' ? "SHIFTS A CELL" : kd == 'h' ? "HINGES A CELL" : "TURNS A CELL");
  }
  int gate = 0, tele = 0, arrow = 0, light = 0;
  for (int c = 0; c < 2 * g->lv->nsq * NN; c++) if (g->drv[c] == s) {
    gate |= g->kind[c] == FK_GATE; tele |= g->kind[c] == FK_TELE; arrow |= g->kind[c] == FK_ONEWAY; light |= g->kind[c] == FK_LIGHT;
  }
  const char *outs[4] = { gate ? "WORKS A GATE" : NULL, tele ? "SWITCHES A TELEPORTER" : NULL, arrow ? "TURNS AN ARROW ROUND" : NULL, light ? "WORKS A LIGHT" : NULL };
  for (int k = 0; k < 4; k++) if (outs[k]) { cat(o, len, moves++ ? " AND " : n++ ? ". IT " : " IT "); cat(o, len, outs[k]); }
  if (moves) n = 1;
  for (int k = 0; k < g->lv->nlogic; k++) for (int i = 0; i < g->lv->logic[k].n; i++) if (g->lv->logic[k].in[i] == s) {
    const FoldLogic *L = &g->lv->logic[k]; char b[48];
    switch (L->op) {
      case FL_AND: snprintf(b, sizeof b, "IT IS 1 OF %d NEEDED TOGETHER", L->n); break;
      case FL_OR: snprintf(b, sizeof b, "IT IS 1 OF %d: ANY WILL DO", L->n); break;
      case FL_XOR: snprintf(b, sizeof b, "IT IS 1 OF %d: ONLY 1 AT A TIME", L->n); break;
      case FL_NOT: snprintf(b, sizeof b, "SOMETHING HAPPENS WHILE IT IS OFF"); break;
      case FL_LATCH: snprintf(b, sizeof b, i ? "IT RESETS SOMETHING" : "ONCE IS ENOUGH"); break;
      case FL_DELAY: snprintf(b, sizeof b, L->count > 1 ? "IT ACTS %d MOVES LATE" : "IT ACTS A MOVE LATE", L->count); break;
      default: snprintf(b, sizeof b, "IT NEEDS %d GOES", L->count); break;
    }
    cat(o, len, n++ ? ". " : " "); cat(o, len, b);
  }
  if (!n) cat(o, len, " IT DOES NOTHING");
  cat(o, len, ".");
}
int fold_describe(const FoldGame *g, int c, char *o, int len) {
  o[0] = 0;
  if (c < 0) return 0;
  if (g->txt[c]) { cat(o, len, g->lv->text[g->txt[c] - 1]); return 1; }
  if (g->box[c] && !g->btn[c]) { cat(o, len, "A BOX. PUSH IT ONTO A TARGET."); return 1; }
  int s = g->btn[c], k = g->kind[c];
  if (k == FK_LIGHT && !g->wall[c]) {         // a light: on or off, what lights it, what it does
    cat(o, len, g->aux[c] & 0x80 ? "A LIGHT. IT IS ON." : "A LIGHT. IT IS OFF.");
    if (g->drv[c]) {
      int drawnOn = ((g->aux[c] & 0x80) != 0) ^ (g->sig[g->drv[c]] != 0);
      cat(o, len, drawnOn ? " IT GOES OUT WHEN " : " IT LIGHTS WHEN "); cond(g, g->drv[c], o, len, 0); cat(o, len, ".");
    } else cat(o, len, " NOTHING WORKS IT.");
    if (s) { cat(o, len, " WHILE IT IS ON"); effect(g, s, o, len); }
    return 1;
  }
  if (s && !g->wall[c]) {                     // something that sends a signal
    cat(o, len, k == FK_PLATE ? "A PLATE. ON WHILE A BOX IS ON IT." : k == FK_SENSOR ? "A SENSOR. ON WHILE YOU STAND ON IT." :
               k == FK_KEY ? "A KEY. TAKING IT DOES SOMETHING." : g->target[c] ? "A TARGET. A BOX ON IT DOES SOMETHING." :
               "A BUTTON. A BOX ON IT SWITCHES IT ON OR OFF.");
    effect(g, s, o, len);
    return 1;
  }
  if (g->wall[c]) return 0;
  switch (k) {
    case FK_ICE: cat(o, len, "ICE. YOU AND BOXES SLIDE ACROSS IT."); break;
    case FK_ONEWAY: cat(o, len, "ONE WAY ONLY."); break;
    case FK_PIT: cat(o, len, "A PIT. A BOX CAN FILL IT."); break;
    case FK_TELE: cat(o, len, g->aux[c] & 0x80 ? "A TELEPORTER. IT IS OFF." : "A TELEPORTER. IT TAKES YOU TO ITS PARTNER."); break;
    case FK_GATE: cat(o, len, g->aux[c] & 0x40 ? (g->aux[c] & 0x80 ? "A GAP IN THE WALL." : "A WALL. IT LOOKS ODD.") : g->aux[c] & 0x80 ? "A GATE. IT IS OPEN." : "A GATE. IT IS SHUT."); break;
    case FK_KEY: cat(o, len, g->aux[c] & 0x80 ? "" : "A KEY. TAKE IT."); break;
    case FK_LOCK: cat(o, len, "A LOCK. IT NEEDS A KEY."); break;
    case FK_GRATE: cat(o, len, "A GRATE. YOU CAN CROSS IT. BOXES CAN'T."); break;
    case FK_SENSOR: cat(o, len, "A SENSOR. IT ISN'T WIRED TO ANYTHING."); break;
    default: if (g->target[c]) cat(o, len, "A TARGET. PUT A BOX ON IT.");
  }
  if (g->drv[c] && (k == FK_GATE || k == FK_TELE || k == FK_ONEWAY)) {   // and what works it
    int drawnOpen = ((g->aux[c] & 0x80) != 0) ^ (g->sig[g->drv[c]] != 0);   // as drawn = now, unless its signal is on
    cat(o, len, k == FK_GATE ? (drawnOpen ? " IT SHUTS WHEN " : " IT OPENS WHEN ") : k == FK_TELE ? (drawnOpen ? " IT SWITCHES OFF WHEN " : " IT SWITCHES ON WHEN ") : " IT TURNS ROUND WHEN ");
    cond(g, g->drv[c], o, len, 0);
    cat(o, len, ".");
  }
  return o[0] != 0;
}

// ---- level files -------------------------------------------------------------------------------------------------
static int parse_act(const char *w, FoldAct *a) {
  memset(a, 0, sizeof *a);
  a->kind = w[0];
  if (!strchr("frRshg", a->kind)) return 0;
  char *end; long i = strtol(w + 1, &end, 10);
  if (end == w + 1 || i < 0 || i >= FOLD_MAXSQ || (a->kind == 'g' && (i < 1 || i > 127))) return 0;
  a->sq = (uint8_t)i;
  if (a->kind == 's' || a->kind == 'h') {
    if (*end != ':' || !end[1]) return 0;
    strncpy(a->spec, end + 1, 2);
    if (a->kind == 's' && !strchr("udlroi", a->spec[0])) return 0;
    if (a->kind == 'h' && (a->spec[0] < '0' || a->spec[0] > '3' || (a->spec[1] != '+' && a->spec[1] != '-'))) return 0;
  }
  return 1;
}
#define FAIL(...) do { if (err) snprintf(err, errlen, __VA_ARGS__); return 0; } while (0)
int fold_parse(FoldLevel *lv, const char *text, char *err, int errlen) {
  memset(lv, 0, sizeof *lv);
  memset(lv->wall, 1, sizeof lv->wall);
  lv->player = -1;
  int lineno = 0, roomFace = -1, roomRow = 0;
  const char *s = text;
  while (*s) {
    char line[FOLD_MAXSOL + 16]; int len = 0;
    while (*s && *s != '\n') { if (len < (int)sizeof line - 1 && *s != '\r') line[len++] = *s; s++; }
    if (*s) s++;
    line[len] = 0; lineno++;
    if (roomFace >= 0) {                      // inside a room block: 8 rows of 8 cells
      if (len < N) FAIL("line %d: room row too short", lineno);
      for (int x = 0; x < N; x++) {
        int c = roomFace * NN + roomRow * N + x; char ch = line[x];
        if (!strchr("#.xbBpPoO~^>v<_123456789=|:kl+*%;@$", ch)) FAIL("line %d: unknown tile '%c'", lineno, ch);
        if (ch == '@' || ch == '$') { lv->kind[c] = FK_LIGHT; lv->aux[c] = ch == '$' ? 0x80 : 0; }   // a light (drawn lit: $)
        if (ch == '*') lv->kind[c] = FK_SENSOR;
        if (ch == '%' || ch == ';') { lv->kind[c] = FK_GATE; lv->aux[c] = (uint8_t)(0x40 | (ch == ';' ? 0x80 : 0)); }   // brick-look gate
        if (ch == '+') lv->kind[c] = FK_GRATE;
        if (ch == 'k') lv->kind[c] = FK_KEY;
        if (ch == 'l') lv->kind[c] = FK_LOCK;
        if (ch == '|' || ch == ':') { lv->kind[c] = FK_GATE; lv->aux[c] = ch == ':' ? 0x80 : 0; }   // its group: a gate line
        if (ch == '=') { if (lv->nbtn == FOLD_MAXSIG - 1) FAIL("line %d: too many signals", lineno); lv->kind[c] = FK_PLATE; lv->btn[c] = (uint8_t)++lv->nbtn; }
        if (ch >= '1' && ch <= '9') { lv->kind[c] = FK_TELE; lv->aux[c] = (uint8_t)(ch - '0'); }
        if (ch == '_') lv->kind[c] = FK_PIT;
        if (ch == '~') lv->kind[c] = FK_ICE;
        if (strchr("^>v<", ch)) { lv->kind[c] = FK_ONEWAY; lv->aux[c] = (uint8_t)(strchr("^>v<", ch) - "^>v<"); }
        lv->wall[c] = ch == '#';
        if (strchr("xBP", ch)) lv->target[c] = 1;
        if (strchr("bBO", ch)) lv->box[c] = 1;
        if (strchr("oO", ch)) { if (lv->nbtn == FOLD_MAXSIG - 1) FAIL("line %d: too many signals", lineno); lv->btn[c] = (uint8_t)++lv->nbtn; }
        if (strchr("pP", ch)) lv->player = c;
      }
      if (++roomRow == N) roomFace = -1;
      continue;
    }
    char *hash = strchr(line, '#'); if (hash) *hash = 0;
    char word[16] = ""; int at = 0;
    if (sscanf(line, " %15s %n", word, &at) < 1) continue;
    const char *rest = line + at;
    if (!strcmp(word, "fold")) { if (atoi(rest) != 1) FAIL("line %d: unknown version", lineno); }
    else if (!strcmp(word, "name")) { strncpy(lv->name, rest, sizeof lv->name - 1); }
    else if (!strcmp(word, "square")) {
      int a[9];
      if (sscanf(rest, "%d %d %d %d %d %d %d %d %d", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8]) != 9) FAIL("line %d: square needs 9 numbers", lineno);
      if (lv->nsq == FOLD_MAXSQ) FAIL("line %d: too many squares", lineno);
      FoldSquare *q = &lv->sq[lv->nsq++];
      for (int i = 0; i < 3; i++) { q->o[i] = (int8_t)a[i]; q->u[i] = (int8_t)a[3 + i]; q->v[i] = (int8_t)a[6 + i]; }
      V3 n = cross(v3(q->v), v3(q->u));
      if (dot(n, n) != 1 || dot(v3(q->u), v3(q->v)) != 0) FAIL("line %d: u and v must be different unit grid axes", lineno);
      for (int i = 0; i < 3; i++) q->n[i] = (int8_t)n.x[i];
    }
    else if (!strcmp(word, "room")) {
      int i; char side;
      if (sscanf(rest, "%d %c", &i, &side) != 2 || i < 0 || i >= lv->nsq || (side != 'f' && side != 'b')) FAIL("line %d: room <square> <f|b>", lineno);
      roomFace = 2 * i + (side == 'b'); roomRow = 0;
    }
    else if (!strcmp(word, "link")) {
      int b; int n = 0;
      if (sscanf(rest, "%d %n", &b, &n) < 1 || b < 1 || b >= FOLD_MAXSIG) FAIL("line %d: link <signal> <moves...>", lineno);
      const char *w = rest + n; char tok[16];
      while (sscanf(w, " %15s%n", tok, &n) == 1) {
        if (lv->nact[b - 1] == FOLD_MAXACT) FAIL("line %d: too many moves", lineno);
        FoldAct *a = &lv->act[b - 1][lv->nact[b - 1]++];
        if (!parse_act(tok, a) || (a->kind != 'g' && a->sq >= lv->nsq)) FAIL("line %d: bad move '%s'", lineno, tok);
        if (a->kind == 'g') { lv->nact[b - 1]--; w += n; continue; }   // old files: gates now follow their own 'drive'
        if ((a->kind == 's' || a->kind == 'h') && !lv->geo[a->sq]) { lv->geo[a->sq] = 1; lv->geoAct[a->sq] = *a; }
        w += n;
      }
    }
    else if (!strcmp(word, "gate")) {           // a gate's group (the button that works it)
      int i, x, y, k; char side;
      if (sscanf(rest, "%d %c %d %d %d", &i, &side, &x, &y, &k) != 5 || i < 0 || i >= lv->nsq || x < 0 || y < 0 || x >= N || y >= N || k < 0 || k > 127) FAIL("line %d: gate <square> <f|b> <x> <y> <button>", lineno);
      int c = (2 * i + (side == 'b')) * NN + y * N + x;
      if (lv->kind[c] != FK_GATE) FAIL("line %d: no gate there", lineno);
      lv->drv[c] = (uint8_t)k;                 // old name for 'drive'
    }
    else if (!strcmp(word, "signal") || !strcmp(word, "drive") || !strcmp(word, "text")) {   // a tile's signal / text
      int i, x, y, k = 0, n = 0; char side;
      if (sscanf(rest, "%d %c %d %d %n", &i, &side, &x, &y, &n) < 4 || i < 0 || i >= lv->nsq || x < 0 || y < 0 || x >= N || y >= N) FAIL("line %d: %s <square> <f|b> <x> <y> ...", lineno, word);
      int c = (2 * i + (side == 'b')) * NN + y * N + x;
      if (word[0] == 't') {
        if (lv->ntext == FOLD_MAXTEXT) FAIL("line %d: too many texts", lineno);
        strncpy(lv->text[lv->ntext], rest + n, FOLD_TEXTLEN - 1); lv->txt[c] = (uint8_t)++lv->ntext;
      } else {
        if (sscanf(rest + n, "%d", &k) != 1 || k < 0 || k >= FOLD_MAXSIG) FAIL("line %d: signal 0..63", lineno);
        if (word[0] == 's') lv->btn[c] = (uint8_t)k; else lv->drv[c] = (uint8_t)k;
      }
    }
    else if (!strcmp(word, "input")) {           // a level-wide input
      int k; char what[16];
      int n = 0;
      if (sscanf(rest, "%d %15s %d", &k, what, &n) < 2 || k < 1 || k >= FOLD_MAXSIG) FAIL("line %d: input <signal> alltargets | tick <n> | moves <n>", lineno);
      if (!strcmp(what, "alltargets")) lv->allSig = k;
      else if (!strcmp(what, "tick") || !strcmp(what, "moves")) {
        if (n < 1 || n > 255 || lv->ntimer == FOLD_MAXTIMER) FAIL("line %d: %s <n> (1..255, 8 timers)", lineno, what);
        FoldTimer *T = &lv->timer[lv->ntimer++]; T->kind = what[0] == 't' ? FT_TICK : FT_MOVES; T->sig = (uint8_t)k; T->n = (uint8_t)n;
      } else FAIL("line %d: input <signal> alltargets | tick <n> | moves <n>", lineno);
    }
    else if (!strcmp(word, "logic")) {           // logic <out> <and|or|xor|not|latch|count [n]> <in...>
      if (lv->nlogic == FOLD_MAXLOGIC) FAIL("line %d: too many logic pieces", lineno);
      FoldLogic *L = &lv->logic[lv->nlogic]; memset(L, 0, sizeof *L);
      char op[8]; int out, n = 0;
      if (sscanf(rest, "%d %7s %n", &out, op, &n) < 2 || out < 1 || out >= FOLD_MAXSIG) FAIL("line %d: logic <out> <op> <in...>", lineno);
      static const char *OPS[] = { "and", "or", "xor", "not", "latch", "count", "delay" };
      L->op = 255; for (int k = 0; k < 7; k++) if (!strcmp(op, OPS[k])) L->op = (uint8_t)k;
      if (L->op == 255) FAIL("line %d: logic op is and / or / xor / not / latch / count / delay", lineno);
      const char *w = rest + n; int v, m;
      if (L->op == FL_COUNT || L->op == FL_DELAY) {
        if (sscanf(w, "%d %n", &v, &m) < 1 || v < 1 || v > (L->op == FL_DELAY ? 8 : 255)) FAIL("line %d: %s <n> <in>", lineno, op);
        L->count = (uint8_t)v; w += m;
      }
      while (L->n < 4 && sscanf(w, "%d%n", &v, &m) == 1) { if (v < 1 || v >= FOLD_MAXSIG) FAIL("line %d: signal 1..63", lineno); L->in[L->n++] = (uint8_t)v; w += m; }
      if (!L->n) FAIL("line %d: logic needs inputs", lineno);
      L->out = (uint8_t)out; lv->nlogic++;
    }
    else if (!strcmp(word, "box") || !strcmp(word, "player")) {   // a box / the player on any tile
      int i, x, y; char side;
      if (sscanf(rest, "%d %c %d %d", &i, &side, &x, &y) != 4 || i < 0 || i >= lv->nsq || x < 0 || y < 0 || x >= N || y >= N) FAIL("line %d: %s <square> <f|b> <x> <y>", lineno, word);
      int c = (2 * i + (side == 'b')) * NN + y * N + x;
      if (word[0] == 'b') lv->box[c] = 1; else lv->player = c;
    }
    else if (!strcmp(word, "sol")) {
      for (const char *c = rest; *c; c++) if (*c >= '0' && *c <= '3' && lv->nsol < FOLD_MAXSOL) lv->sol[lv->nsol++] = *c;
    }
    else FAIL("line %d: unknown item '%s'", lineno, word);
  }
  if (roomFace >= 0) FAIL("room block cut short");
  if (!lv->nsq) FAIL("no squares");
  if (lv->player < 0) FAIL("no player");
  return 1;
}
