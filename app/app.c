// FOLD app: the level editor (level view, cell editor, cell settings), playtesting in the VMX window, and the menu.
// Words (owner, 2026-10-20): LEVEL = the whole level; CELL = one room square (a "square" / face pair in the core);
// TILE = one element inside a cell (a wall, floor, box, target, button, the player; a "cell" in the core's arrays).
// Controls (the owner's design, 2026-10-20):
//   shape view: left stick moves the cursor cell on the working plane; A places a tile (facing you), B removes it;
//     the D-pad folds the working plane 90 degrees about the cursor cell's edge that way, wrapping away from you (round
//     the shape); the cursor moves onto the new plane and the view turns to face it; L / R turn the view 90 degrees, the right stick turns / tilts it freely (with BACK held it zooms); X opens the
//     room on the side facing you; Y opens the tile's settings (its room move, the button that triggers it, the back
//     room); START opens the menu.
//   room editor: left stick moves the cell cursor (off an edge it goes into the next room); the D-pad jumps to the
//     next room that way; A paints the item, B clears to floor, L / R choose the item; doorways on the outer ring are
//     mirrored into the room beyond; X goes back to the shape view.
#include "app.h"
#include "gfx.h"
#include "vmx.h"
#include "../core/fold.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define N FOLD_N
#define NN FOLD_NN
#define PI 3.14159265f
enum { M_SHAPE, M_ROOM, M_PROPS, M_MENU, M_PLAY };

// colours (TV)
#define C_BG 0x0b0e0a
#define C_GRID 0x1e3318
#define C_EDGE 0x3f7f2c
#define C_CURSOR 0xd8ff9c
#define C_TEXT 0xd9e3d2
#define C_DIM 0x8c9787
#define C_HI 0x7dff3a

// ---- the level being built ---------------------------------------------------------------------------------------
static FoldLevel L;                       // squares and room cells (btn: 1 = a button; numbered when built)
static FoldSurface S;                     // joins for the squares where they are
static uint8_t sides[FOLD_MAXSQ];         // bit 0 front is a room, bit 1 back is a room
static FoldAct func[FOLD_MAXSQ];          // a tile's room move (kind 0 = none)
static int trig[FOLD_MAXSQ];              // the button that triggers it (0 = none)
static char levelsDir[256] = "levels", path[300] = "", msg[64] = "";
static int msgT, mode = M_SHAPE, quit, t;

// cursor cell (shape view): corner co, axes ce1 ce2 (positive unit grid vectors)
static int co[3], ce1[3] = { 1, 0, 0 }, ce2[3] = { 0, 1, 0 };
// camera: orbit round the cursor
static float yaw = -PI / 2, yawGoal = -PI / 2, pitch = 0.9f, dist = 5.5f;
static Vec3 look = { 0.5f, 0.5f, 0 }, eye;
static Mat4 VPm;
// room editor
static int rf, rrot, rx = 3, ry = 3, item = 1;
static const char *ITEMS[] = { "WALL", "FLOOR", "BOX", "TARGET", "BUTTON", "PLAYER", "ICE", "ONE-WAY", "PIT", "TELEPORTER", "PLATE", "GATE", "KEY", "LOCK", "GRATE" };
#define NITEMS 15
// settings / menu
static int prow, mrow, fileIx, vmxWin = 1, propSq, propsBack = M_SHAPE;
// playtest
static FoldLevel PL; static FoldGame G; static int popT, wonT; static const char *popWord;

static void say(const char *s) { snprintf(msg, sizeof msg, "%s", s); msgT = 90; }

static int center2_eq(const FoldSquare *q, const int *o, const int *a, const int *b) {
  for (int i = 0; i < 3; i++) if (2 * q->o[i] + q->u[i] + q->v[i] != 2 * o[i] + a[i] + b[i]) return 0;
  int n0 = a[1] * b[2] - a[2] * b[1], n1 = a[2] * b[0] - a[0] * b[2], n2 = a[0] * b[1] - a[1] * b[0];
  return abs(q->n[0] * n0 + q->n[1] * n1 + q->n[2] * n2) == 1;
}
static int find_sq(const int *o, const int *a, const int *b) { for (int i = 0; i < L.nsq; i++) if (center2_eq(&L.sq[i], o, a, b)) return i; return -1; }
static void make_room(int f) {           // a fresh room: wall round the outside, floor inside
  for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
    int c = f * NN + y * N + x;
    L.wall[c] = x == 0 || y == 0 || x == N - 1 || y == N - 1; L.target[c] = L.box[c] = L.btn[c] = L.kind[c] = L.aux[c] = 0;
  }
  sides[f >> 1] |= 1 << (f & 1);
}
static void clear_face(int f) { for (int c = f * NN; c < (f + 1) * NN; c++) { L.wall[c] = 1; L.target[c] = L.box[c] = L.btn[c] = L.kind[c] = L.aux[c] = 0; } if (L.player / NN == f) L.player = -1; }
static void resurface(void) { fold_surface(&S, L.sq, L.nsq); }
static void new_level(void) {
  memset(&L, 0, sizeof L); memset(L.wall, 1, sizeof L.wall); L.player = -1;
  memset(sides, 0, sizeof sides); memset(func, 0, sizeof func); memset(trig, 0, sizeof trig);
  strcpy(L.name, "NEW LEVEL"); path[0] = 0;
  int o[3] = { 0, 0, 0 }; memcpy(co, o, sizeof co);
  ce1[0] = 1; ce1[1] = ce1[2] = 0; ce2[1] = 1; ce2[0] = ce2[2] = 0;
  resurface();
}
static int add_sq(int nc[3]) {           // a tile at the cursor, its front facing nc
  if (L.nsq == FOLD_MAXSQ) { say("TOO MANY CELLS"); return -1; }
  FoldSquare q; memset(&q, 0, sizeof q);
  const int *u = ce1, *v = ce2;
  int c0 = v[1] * u[2] - v[2] * u[1], c1 = v[2] * u[0] - v[0] * u[2], c2 = v[0] * u[1] - v[1] * u[0];   // v x u
  if (c0 * nc[0] + c1 * nc[1] + c2 * nc[2] < 0) { u = ce2; v = ce1; }
  for (int i = 0; i < 3; i++) { q.o[i] = (int8_t)co[i]; q.u[i] = (int8_t)u[i]; q.v[i] = (int8_t)v[i]; q.n[i] = (int8_t)nc[i]; }
  int i = L.nsq++;
  L.sq[i] = q; sides[i] = 0; func[i].kind = 0; trig[i] = 0;
  make_room(2 * i); clear_face(2 * i + 1);
  resurface();
  return i;
}
static void remove_sq(int i) {
  int n = L.nsq;
  uint8_t *arr[6] = { L.wall, L.target, L.box, L.btn, L.kind, L.aux };
  for (int k = 0; k < 6; k++) memmove(arr[k] + 2 * i * NN, arr[k] + 2 * (i + 1) * NN, (size_t)(2 * (n - i - 1) * NN));
  memmove(&L.sq[i], &L.sq[i + 1], sizeof(FoldSquare) * (n - i - 1));
  memmove(&sides[i], &sides[i + 1], (size_t)(n - i - 1)); memmove(&func[i], &func[i + 1], sizeof(FoldAct) * (n - i - 1));
  memmove(&trig[i], &trig[i + 1], sizeof(int) * (n - i - 1));
  for (int k = 0; k < n - 1; k++) if (func[k].kind && func[k].sq > i) func[k].sq--;
  if (L.player >= 0) { int f = L.player / NN; if (f >> 1 == i) L.player = -1; else if (f >> 1 > i) L.player -= 2 * NN; }
  L.nsq--;
  for (int k = 0; k < 6; k++) memset(arr[k] + 2 * L.nsq * NN, k == 0, 2 * NN);
  resurface();
}
static int nbuttons(void) { int n = 0; for (int c = 0; c < 2 * L.nsq * NN; c++) if (L.btn[c] && !L.wall[c]) n++; return n; }
// the level as the core wants it: buttons numbered in room order, links from the tiles' moves
static void build(FoldLevel *out) {
  *out = L;
  int n = 0;
  for (int c = 0; c < 2 * L.nsq * NN; c++) out->btn[c] = L.btn[c] && !L.wall[c] ? (uint8_t)++n : 0;
  out->nbtn = n; memset(out->nact, 0, sizeof out->nact); memset(out->geo, 0, sizeof out->geo);
  for (int i = 0; i < L.nsq; i++) {
    if (!func[i].kind || trig[i] < 1 || trig[i] > n) continue;
    FoldAct a = func[i]; a.sq = (uint8_t)i;
    out->act[trig[i] - 1][out->nact[trig[i] - 1]++] = a;
    if ((a.kind == 's' || a.kind == 'h') && !out->geo[i]) { out->geo[i] = 1; out->geoAct[i] = a; }
  }
  for (int b = 1; b <= n; b++) {              // a button works the gates that name it
    int any = 0;
    for (int c = 0; c < 2 * L.nsq * NN; c++) if (L.kind[c] == FK_GATE && !L.wall[c] && (L.aux[c] & 0x7f) == b) any = 1;
    if (any && out->nact[b - 1] < FOLD_MAXACT) { FoldAct a; memset(&a, 0, sizeof a); a.kind = 'g'; a.sq = (uint8_t)b; out->act[b - 1][out->nact[b - 1]++] = a; }
  }
  out->nsol = 0;
}

// ---- files -------------------------------------------------------------------------------------------------------
int app_save(const char *p) {
  FoldLevel B; build(&B);
  FILE *f = fopen(p, "w"); if (!f) return 0;
  fprintf(f, "fold 1\nname %s\n", L.name);
  for (int i = 0; i < L.nsq; i++) { const FoldSquare *q = &L.sq[i];
    fprintf(f, "square %d %d %d  %d %d %d  %d %d %d\n", q->o[0], q->o[1], q->o[2], q->u[0], q->u[1], q->u[2], q->v[0], q->v[1], q->v[2]); }
  for (int fc = 0; fc < 2 * L.nsq; fc++) {
    if (!(sides[fc >> 1] >> (fc & 1) & 1)) continue;
    fprintf(f, "room %d %c\n", fc >> 1, fc & 1 ? 'b' : 'f');
    for (int y = 0; y < N; y++) { for (int x = 0; x < N; x++) { int c = fc * NN + y * N + x;
      char ch = L.kind[c] == FK_ICE ? '~' : L.kind[c] == FK_ONEWAY ? "^>v<"[L.aux[c] & 3] : L.kind[c] == FK_PIT ? '_' : L.kind[c] == FK_TELE ? (char)('0' + L.aux[c]) : L.kind[c] == FK_PLATE ? '=' : L.kind[c] == FK_GATE ? (L.aux[c] & 0x80 ? ':' : '|') : L.kind[c] == FK_KEY ? 'k' : L.kind[c] == FK_LOCK ? 'l' : L.kind[c] == FK_GRATE ? '+' :   // special floors: a box /
        // the player on them goes on its own line
        L.wall[c] ? '#' : L.box[c] ? (L.target[c] ? 'B' : B.btn[c] ? 'O' : 'b') : c == L.player ? (L.target[c] ? 'P' : 'p') : L.target[c] ? 'x' : B.btn[c] ? 'o' : '.';
      fputc(ch, f); } fputc('\n', f); }
    for (int c = fc * NN; c < (fc + 1) * NN; c++) if (L.kind[c] && !L.wall[c]) {
      if (L.kind[c] == FK_GATE && (L.aux[c] & 0x7f)) fprintf(f, "gate %d %c %d %d %d\n", fc >> 1, fc & 1 ? 'b' : 'f', c % N, (c / N) % N, L.aux[c] & 0x7f);
      if (L.box[c]) fprintf(f, "box %d %c %d %d\n", fc >> 1, fc & 1 ? 'b' : 'f', c % N, (c / N) % N);
      if (c == L.player) fprintf(f, "player %d %c %d %d\n", fc >> 1, fc & 1 ? 'b' : 'f', c % N, (c / N) % N);
    }
  }
  for (int b = 0; b < B.nbtn; b++) if (B.nact[b]) {
    fprintf(f, "link %d", b + 1);
    for (int k = 0; k < B.nact[b]; k++) { const FoldAct *a = &B.act[b][k];
      if (a->kind == 's' || a->kind == 'h') fprintf(f, " %c%d:%s", a->kind, a->sq, a->spec); else fprintf(f, " %c%d", a->kind, a->sq); }   // g<k> too
    fputc('\n', f);
  }
  fclose(f);
  return 1;
}
int app_load(const char *p) {
  FILE *f = fopen(p, "rb"); if (!f) return 0;
  static char text[1 << 16]; size_t n = fread(text, 1, sizeof text - 1, f); text[n] = 0; fclose(f);
  static FoldLevel tmp; char err[96];
  if (!fold_parse(&tmp, text, err, sizeof err)) { say(err); return 0; }
  new_level(); L = tmp; snprintf(path, sizeof path, "%s", p);
  for (int i = 0; i < L.nsq; i++) {        // which sides are rooms: any floor in them
    sides[i] = 0;
    for (int s = 0; s < 2; s++) for (int c = (2 * i + s) * NN; c < (2 * i + s + 1) * NN; c++) if (!L.wall[c]) { sides[i] |= 1 << s; break; }
  }
  for (int b = 0; b < L.nbtn; b++) for (int k = 0; k < L.nact[b]; k++) { const FoldAct *a = &L.act[b][k]; if (a->kind == 'g') continue; func[a->sq] = *a; trig[a->sq] = b + 1; }
  for (int c = 0; c < 2 * L.nsq * NN; c++) L.btn[c] = L.btn[c] ? 1 : 0;
  // cursor on square 0, flat in its plane
  const FoldSquare *q = &L.sq[0];
  for (int i = 0; i < 3; i++) { co[i] = q->o[i] + (q->u[i] < 0 ? q->u[i] : 0) + (q->v[i] < 0 ? q->v[i] : 0); ce1[i] = abs(q->u[i]); ce2[i] = abs(q->v[i]); }
  resurface();
  return 1;
}
static int list_levels(char names[][64], int max) {
  DIR *d = opendir(levelsDir); int n = 0; if (!d) return 0;
  struct dirent *e;
  while ((e = readdir(d)) && n < max) { size_t l = strlen(e->d_name); if (l > 5 && !strcmp(e->d_name + l - 5, ".fold")) snprintf(names[n++], 64, "%s", e->d_name); }
  closedir(d);
  for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (strcmp(names[j], names[i]) < 0) { char tmp[64]; strcpy(tmp, names[i]); strcpy(names[i], names[j]); strcpy(names[j], tmp); }
  return n;
}

// ---- input helpers -----------------------------------------------------------------------------------------------
static unsigned prev, hold[12];
static int pressed(const Pad *p, unsigned bit) { return (p->held & bit) && !(prev & bit); }
static int dpad_rep(const Pad *p, int *dir) {   // a D-pad press, repeating while held
  for (int k = 0; k < 4; k++) {
    unsigned bit = (unsigned)1 << k;
    if (p->held & bit) { hold[k]++; if (hold[k] == 1 || (hold[k] > 12 && hold[k] % 4 == 0)) { *dir = k; return 1; } }
    else hold[k] = 0;
  }
  return 0;
}
static int stickT;
static int stick_rep(float x, float y, int *dir) {   // the stick as 4 directions, repeating
  float ax = fabsf(x), ay = fabsf(y);
  if (ax < 0.5f && ay < 0.5f) { stickT = 0; return 0; }
  stickT++;
  if (stickT != 1 && !(stickT > 8 && stickT % 4 == 0)) return 0;
  *dir = ax > ay ? (x > 0 ? 3 : 2) : (y > 0 ? 1 : 0);    // PAD bit order: 0 up 1 down 2 left 3 right
  return 1;
}
static const int PADDIR[4] = { 0, 2, 3, 1 };   // PAD bit order -> screen direction (0 up 1 right 2 down 3 left)
static const float SX[4] = { 0, 1, 0, -1 }, SY[4] = { -1, 0, 1, 0 };

// ---- camera and screen directions ---------------------------------------------------------------------------------
static Vec3 cur_centre(void) { return v3f(co[0] + (ce1[0] + ce2[0]) * 0.5f, co[1] + (ce1[1] + ce2[1]) * 0.5f, co[2] + (ce1[2] + ce2[2]) * 0.5f); }
static void camera(void) {
  Vec3 c = cur_centre();
  look.x += (c.x - look.x) * 0.25f; look.y += (c.y - look.y) * 0.25f; look.z += (c.z - look.z) * 0.25f;
  yaw += (yawGoal - yaw) * 0.25f;
  eye = v3f(look.x + dist * cosf(pitch) * cosf(yaw), look.y + dist * cosf(pitch) * sinf(yaw), look.z + dist * sinf(pitch));
  Vec3 up = v3f(0, 0, 1);
  if (fabsf(pitch) > 1.5f) up = v3f(-cosf(yaw), -sinf(yaw), 0);
  VPm = mat_mul(mat_perspective(1.05f, (float)GW / GH, 0.05f, 100.0f), mat_lookat(eye, look, up));
  gfx_camera(VPm);
}
// of the cursor plane's 4 directions (+-e1, +-e2), the one pointing most along screen direction sd
static void plane_dir(int sd, int out[3]) {
  Vec3 c = cur_centre(); float cx, cy, best = -1e9f;
  gfx_project(c, &cx, &cy);
  for (int k = 0; k < 4; k++) {
    const int *a = k < 2 ? ce1 : ce2; int sg = k & 1 ? -1 : 1;
    float x, y;
    if (!gfx_project(v3f(c.x + sg * a[0] * 0.5f, c.y + sg * a[1] * 0.5f, c.z + sg * a[2] * 0.5f), &x, &y)) continue;
    float dx = x - cx, dy = y - cy, l = sqrtf(dx * dx + dy * dy) + 1e-6f, s = (dx * SX[sd] + dy * SY[sd]) / l;
    if (s > best) { best = s; for (int i = 0; i < 3; i++) out[i] = sg * a[i]; }
  }
}
static void facing_normal(int nc[3]) {   // the cursor plane's normal on the camera's side
  int n[3] = { ce1[1] * ce2[2] - ce1[2] * ce2[1], ce1[2] * ce2[0] - ce1[0] * ce2[2], ce1[0] * ce2[1] - ce1[1] * ce2[0] };
  Vec3 c = cur_centre(); float d = (eye.x - c.x) * n[0] + (eye.y - c.y) * n[1] + (eye.z - c.z) * n[2];
  for (int i = 0; i < 3; i++) nc[i] = d < 0 ? -n[i] : n[i];
}
static int sq_face_toward_eye(int i) {   // which side of square i faces the camera
  const FoldSquare *q = &L.sq[i];
  Vec3 c = v3f(q->o[0] + (q->u[0] + q->v[0]) * 0.5f, q->o[1] + (q->u[1] + q->v[1]) * 0.5f, q->o[2] + (q->u[2] + q->v[2]) * 0.5f);
  float d = (eye.x - c.x) * q->n[0] + (eye.y - c.y) * q->n[1] + (eye.z - c.z) * q->n[2];
  return 2 * i + (d < 0);
}

// ---- shape view --------------------------------------------------------------------------------------------------
static void fold_plane(int sd) {
  int a[3], b[3], nc[3]; plane_dir(sd, a); facing_normal(nc);
  int pos = a[0] + a[1] + a[2] > 0;               // folding over the cell's far edge along that axis
  const int *other = (abs(a[0]) == ce1[0] && abs(a[1]) == ce1[1] && abs(a[2]) == ce1[2]) ? ce2 : ce1;
  memcpy(b, other, sizeof b);
  int start[3], away[3];
  for (int i = 0; i < 3; i++) { start[i] = co[i] + (pos ? a[i] : 0); away[i] = -nc[i]; }   // pivot edge's low end
  // the new plane wraps away from you, round the shape (seen from outside, its front faces the way you went)
  for (int i = 0; i < 3; i++) { co[i] = start[i] + (away[i] < 0 ? away[i] : 0); ce1[i] = b[i]; ce2[i] = abs(away[i]); }
  // and the view turns to face it from that side, a little from the side and above
  if (a[2] == 0) {
    float y = atan2f((float)a[1], (float)a[0]) + 0.55f;
    while (y - yawGoal > PI) y -= 2 * PI;
    while (y - yawGoal < -PI) y += 2 * PI;
    yawGoal = y; pitch = 0.45f;
  } else pitch = a[2] > 0 ? 1.0f : -1.0f;
}
static void shape_frame(const Pad *p) {
  int d;
  if (stick_rep(p->lx, p->ly, &d)) {     // move the cursor along the plane
    int a[3]; plane_dir(PADDIR[d], a);
    for (int i = 0; i < 3; i++) co[i] += a[i];
  }
  if (dpad_rep(p, &d)) fold_plane(PADDIR[d]);
  if (pressed(p, PAD_L)) yawGoal -= PI / 2;
  if (pressed(p, PAD_R)) yawGoal += PI / 2;
  if (p->held & PAD_BACK) { dist *= 1 + p->ry * 0.04f; if (dist < 1.2f) dist = 1.2f; if (dist > 20) dist = 20; }   // BACK + right stick: zoom
  else { yawGoal += p->rx * 0.06f; pitch -= p->ry * 0.05f; }
  if (pitch > 1.45f) pitch = 1.45f; if (pitch < -1.45f) pitch = -1.45f;
  int i = find_sq(co, ce1, ce2);
  if (pressed(p, PAD_A)) {
    if (i >= 0) say("A CELL IS ALREADY THERE");
    else { int nc[3]; facing_normal(nc); if (add_sq(nc) >= 0) say("CELL PLACED"); }
  }
  if (pressed(p, PAD_B) && i >= 0) { remove_sq(i); say("CELL REMOVED"); }
  if (pressed(p, PAD_X) && i >= 0) {
    rf = sq_face_toward_eye(i);
    if (!(sides[i] >> (rf & 1) & 1)) make_room(rf);
    rrot = 0; mode = M_ROOM;
  }
  if (pressed(p, PAD_Y) && i >= 0) { propSq = i; prow = 0; propsBack = M_SHAPE; mode = M_PROPS; }
  if (pressed(p, PAD_START)) { mrow = 0; mode = M_MENU; }
}

// ---- room editor -------------------------------------------------------------------------------------------------
static int screen_to_cell(int sx, int sy) {   // inverse of fold_to_screen for the room on screen
  for (int c = rf * NN; c < (rf + 1) * NN; c++) { int x, y; fold_to_screen(c % N, (c / N) % N, rrot, &x, &y); if (x == sx && y == sy) return c; }
  return rf * NN;
}
static int go_room(int c, int sd) {           // step from cell c in screen direction sd into the next room
  int d = (sd + rrot) & 3, d1, c1 = fold_step(&S, c, d, &d1);
  if (c1 < 0) return 0;
  int f1 = c1 / NN;
  if (!(sides[f1 >> 1] >> (f1 & 1) & 1)) return 0;
  rf = f1; rrot = (d1 - sd) & 3;
  fold_to_screen(c1 % N, (c1 / N) % N, rrot, &rx, &ry);
  return 1;
}
static void paint(int c, int what) {
  int x = c % N, y = (c / N) % N, edge = x == 0 || y == 0 || x == N - 1 || y == N - 1, corner = (x == 0 || x == N - 1) && (y == 0 || y == N - 1);
  if (corner && what != 0) { say("CORNERS STAY WALL"); return; }
  switch (what) {
    case 0: if (c == L.player) { say("MOVE THE PLAYER FIRST"); return; } L.wall[c] = 1; L.box[c] = L.target[c] = L.btn[c] = L.kind[c] = L.aux[c] = 0; break;
    case 1: L.wall[c] = 0; L.box[c] = L.target[c] = L.btn[c] = L.kind[c] = L.aux[c] = 0; break;
    case 2: if (c == L.player) return; L.wall[c] = 0; L.box[c] ^= 1; break;
    case 3: L.wall[c] = 0; L.target[c] ^= 1; break;
    case 4: L.wall[c] = 0; L.btn[c] ^= 1; if (L.kind[c] == FK_PLATE) L.kind[c] = FK_FLOOR; break;
    case 5: if (L.box[c]) return; L.wall[c] = 0; L.player = c; break;
    case 6: L.wall[c] = 0; L.btn[c] = 0; L.kind[c] = L.kind[c] == FK_ICE ? FK_FLOOR : FK_ICE; break;   // ice
    case 7:                                   // one-way: placed pointing up the screen; A again turns it
      L.wall[c] = 0; L.btn[c] = 0;
      if (L.kind[c] == FK_ONEWAY) L.aux[c] = (uint8_t)((L.aux[c] + 1) & 3); else { L.kind[c] = FK_ONEWAY; L.aux[c] = (uint8_t)rrot; }
      break;
    case 8: if (c == L.player) return; L.wall[c] = 0; L.btn[c] = L.box[c] = L.target[c] = 0; L.kind[c] = L.kind[c] == FK_PIT ? FK_FLOOR : FK_PIT; break;
    case 11:                                  // gate: placed shut; A again opens / shuts it (how it starts); Y picks its button
      L.wall[c] = 0; L.btn[c] = 0;
      if (L.kind[c] == FK_GATE) L.aux[c] ^= 0x80; else { L.kind[c] = FK_GATE; L.aux[c] = 0; say("Y ON A GATE PICKS ITS BUTTON"); }
      break;
    case 14:                                  // grate: you can cross it, boxes can't
      L.wall[c] = 0; L.btn[c] = 0; L.box[c] = L.target[c] = 0; L.kind[c] = L.kind[c] == FK_GRATE ? FK_FLOOR : FK_GRATE; break;
    case 12: case 13:                         // key / lock
      if (c == L.player && what == 13) return;
      L.wall[c] = 0; L.btn[c] = 0; if (what == 13) L.box[c] = L.target[c] = 0;
      { int k = what == 12 ? FK_KEY : FK_LOCK; L.kind[c] = L.kind[c] == k ? FK_FLOOR : (uint8_t)k; }
      break;
    case 10:                                  // pressure plate: a button that holds only while a box is on it
      L.wall[c] = 0;
      if (L.kind[c] == FK_PLATE) { L.kind[c] = FK_FLOOR; L.btn[c] = 0; } else { L.kind[c] = FK_PLATE; L.btn[c] = 1; }
      break;
    case 9: {                                 // teleporter: pairs with a lone pad if there is one; A again changes its pair
      L.wall[c] = 0; L.btn[c] = 0;
      if (L.kind[c] == FK_TELE) { L.aux[c] = (uint8_t)(L.aux[c] % 9 + 1); break; }
      int count[10] = { 0 }, pick = 0;
      for (int k = 0; k < 2 * L.nsq * NN; k++) if (L.kind[k] == FK_TELE && L.aux[k] >= 1 && L.aux[k] <= 9) count[L.aux[k]]++;
      for (int d = 1; d <= 9 && !pick; d++) if (count[d] == 1) pick = d;
      for (int d = 1; d <= 9 && !pick; d++) if (count[d] == 0) pick = d;
      if (!pick) { say("ALL 9 PAIRS USED"); break; }
      L.kind[c] = FK_TELE; L.aux[c] = (uint8_t)pick;
      char m[32]; snprintf(m, sizeof m, "TELEPORTER PAIR %d", pick); say(m);
      break;
    }
  }
  if (edge && !corner) {                      // a doorway opens (or closes) on both sides
    int d = y == 0 ? 0 : x == N - 1 ? 1 : y == N - 1 ? 2 : 3, d1, m = fold_step(&S, c, d, &d1);
    if (m >= 0 && (sides[(m / NN) >> 1] >> ((m / NN) & 1) & 1)) { L.wall[m] = L.wall[c]; if (L.wall[m]) { L.box[m] = L.target[m] = L.btn[m] = L.kind[m] = L.aux[m] = 0; } }
  }
}
static void room_frame(const Pad *p) {
  int d;
  if (stick_rep(p->lx, p->ly, &d)) {
    int sd = PADDIR[d], nx = rx + (int)SX[sd], ny = ry + (int)SY[sd];
    if (nx >= 0 && ny >= 0 && nx < N && ny < N) { rx = nx; ry = ny; }
    else go_room(screen_to_cell(rx, ry), sd);
  }
  if (dpad_rep(p, &d)) {                      // jump a whole room: from the edge cell that way
    int sd = PADDIR[d], ex = sd == 1 ? N - 1 : sd == 3 ? 0 : rx, ey = sd == 2 ? N - 1 : sd == 0 ? 0 : ry;
    if (!go_room(screen_to_cell(ex, ey), sd)) say("NO CELL THAT WAY");
  }
  if (pressed(p, PAD_L)) item = (item + NITEMS - 1) % NITEMS;
  if (pressed(p, PAD_R)) item = (item + 1) % NITEMS;
  if (pressed(p, PAD_A)) paint(screen_to_cell(rx, ry), item);
  if (pressed(p, PAD_B)) paint(screen_to_cell(rx, ry), 1);
  if (pressed(p, PAD_X)) mode = M_SHAPE;
  if (pressed(p, PAD_Y)) {
    int c = screen_to_cell(rx, ry);
    if (L.kind[c] == FK_GATE && !L.wall[c]) {   // a gate: which button works it (0 = none)
      int n = nbuttons(), k = ((L.aux[c] & 0x7f) + 1) % (n + 1);
      L.aux[c] = (uint8_t)((L.aux[c] & 0x80) | k);
      char m[40]; if (k) snprintf(m, sizeof m, "GATE WORKED BY BUTTON %d", k); else snprintf(m, sizeof m, "GATE: NO BUTTON"); say(m);
    } else { propSq = rf >> 1; prow = 0; propsBack = M_ROOM; mode = M_PROPS; }
  }
  if (pressed(p, PAD_START)) { mrow = 0; mode = M_MENU; }
}

// ---- tile settings -----------------------------------------------------------------------------------------------
static const char *FUNCS[] = { "NONE", "FLIP", "TURN", "SHIFT UP", "SHIFT DOWN", "SHIFT LEFT", "SHIFT RIGHT", "SHIFT OUT",
  "SHIFT IN", "HINGE TOP OUT", "HINGE TOP IN", "HINGE RIGHT OUT", "HINGE RIGHT IN", "HINGE BOTTOM OUT", "HINGE BOTTOM IN",
  "HINGE LEFT OUT", "HINGE LEFT IN" };
#define NFUNCS 17
static FoldAct func_of(int k) {
  FoldAct a; memset(&a, 0, sizeof a);
  if (k == 1) a.kind = 'f'; else if (k == 2) a.kind = 'r';
  else if (k >= 3 && k <= 8) { a.kind = 's'; a.spec[0] = "udlroi"[k - 3]; }
  else if (k >= 9) { a.kind = 'h'; a.spec[0] = (char)('0' + (k - 9) / 2); a.spec[1] = (k - 9) % 2 ? '-' : '+'; }
  return a;
}
static int func_index(const FoldAct *a) {
  for (int k = 0; k < NFUNCS; k++) { FoldAct b = func_of(k); if (b.kind == a->kind && !strcmp(b.spec, a->spec)) return k; }
  return 0;
}
static int func_ok(int i, const FoldAct *a) {   // a shift / hinge must not land the tile on another
  if (a->kind != 's' && a->kind != 'h') return 1;
  FoldSquare tmp[FOLD_MAXSQ]; memcpy(tmp, L.sq, sizeof(FoldSquare) * L.nsq);
  tmp[i] = fold_geo_pose(L.sq[i], a);
  return !fold_overlap(tmp, L.nsq);
}
static void props_frame(const Pad *p) {
  int d, i = propSq;
  if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) {
    int sd = PADDIR[d];
    if (sd == 0) prow = (prow + 3) % 4; else if (sd == 2) prow = (prow + 1) % 4;
    else {
      int st = sd == 1 ? 1 : -1;
      if (prow == 0) {
        int k = func_index(&func[i]);
        for (int tries = 0; tries < NFUNCS; tries++) {
          k = (k + st + NFUNCS) % NFUNCS; FoldAct a = func_of(k); a.sq = (uint8_t)i;
          if (func_ok(i, &a)) { func[i] = a; break; }
        }
      } else if (prow == 1) { int n = nbuttons(); trig[i] = (trig[i] + st + n + 1) % (n + 1); }
      else if (prow == 2) { if (sides[i] & 2) { sides[i] &= ~2; clear_face(2 * i + 1); } else make_room(2 * i + 1); }
    }
  }
  if (pressed(p, PAD_A) && prow == 3) { remove_sq(i); mode = M_SHAPE; say("CELL REMOVED"); return; }
  if (prow == 2 && !(sides[i] & 2) && propsBack == M_ROOM && rf == 2 * i + 1) propsBack = M_SHAPE;   // its room went
  if (pressed(p, PAD_B) || pressed(p, PAD_Y)) mode = propsBack;
}

// ---- menu and playtest -------------------------------------------------------------------------------------------
static const char *MENU[] = { "PLAYTEST", "VMX WINDOW", "SAVE", "LOAD", "NEW LEVEL", "QUIT" };
static char files[64][64]; static int nfiles;
static void start_play(void) {
  build(&PL);
  if (PL.player < 0) { say("PUT THE PLAYER IN A CELL FIRST"); mode = M_SHAPE; return; }
  fold_start(&G, &PL); popT = wonT = 0; mode = M_PLAY;
}
static void menu_frame(const Pad *p) {
  int d;
  if (mrow == 3 && !nfiles) nfiles = list_levels(files, 64);
  if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) {
    int sd = PADDIR[d];
    if (sd == 0) mrow = (mrow + 5) % 6; else if (sd == 2) mrow = (mrow + 1) % 6;
    else if (mrow == 3 && nfiles) fileIx = (fileIx + (sd == 1 ? 1 : nfiles - 1)) % nfiles;
    else if (mrow == 1) vmxWin ^= 1;
  }
  if (pressed(p, PAD_A)) {
    char full[400];
    switch (mrow) {
      case 0: start_play(); return;
      case 1: vmxWin ^= 1; break;
      case 2:
        if (!path[0]) { for (int k = 1; k < 1000; k++) { snprintf(path, sizeof path, "%s/user-%03d.fold", levelsDir, k); FILE *f = fopen(path, "r"); if (!f) break; fclose(f); } }
        say(app_save(path) ? "SAVED" : "COULD NOT SAVE"); nfiles = 0; break;
      case 3: if (nfiles) { snprintf(full, sizeof full, "%s/%s", levelsDir, files[fileIx]); say(app_load(full) ? "LOADED" : "COULD NOT LOAD"); mode = M_SHAPE; } return;
      case 4: new_level(); say("NEW LEVEL"); mode = M_SHAPE; return;
      case 5: quit = 1; return;
    }
  }
  if (pressed(p, PAD_B) || pressed(p, PAD_START)) mode = M_SHAPE;
}
static void play_frame(const Pad *p) {
  if (popT) { popT--; return; }
  if (wonT) { wonT++; if (pressed(p, PAD_A) || pressed(p, PAD_B)) mode = M_SHAPE; return; }
  int d;
  if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) {
    int r = fold_move(&G, PADDIR[d]);
    popWord = r & FOLD_FLIPPED ? "FLIPPED" : r & FOLD_TURNED ? "TURNED" : r & FOLD_SHIFTED ? "SHIFTED" : r & FOLD_HINGED ? "HINGED" : NULL;
    if (popWord) popT = 30;
    if (fold_solved(&G)) wonT = 1;
  }
  if (pressed(p, PAD_A)) fold_undo(&G);
  if (pressed(p, PAD_B) || pressed(p, PAD_START)) mode = M_SHAPE;
}

// ---- frame -------------------------------------------------------------------------------------------------------
void app_init(const char *dir) { snprintf(levelsDir, sizeof levelsDir, "%s", dir); new_level(); }
int app_quit(void) { return quit; }
void app_frame(const Pad *p) {
  t++; if (msgT) msgT--;
  camera();
  switch (mode) {
    case M_SHAPE: shape_frame(p); break;
    case M_ROOM: room_frame(p); break;
    case M_PROPS: props_frame(p); break;
    case M_MENU: menu_frame(p); break;
    case M_PLAY: play_frame(p); break;
  }
  prev = p->held;
}

// ---- drawing -----------------------------------------------------------------------------------------------------
static uint32_t texbuf[2][56 * 56];
static uint8_t bits[64 * 64];
static void room_tex(int f, int rot, uint32_t *out, uint32_t lit, uint32_t dark) {   // the VMX room as a 56x56 texture
  memset(bits, 0, sizeof bits);
  vmx_room(bits, L.wall, L.target, L.box, L.btn, L.kind, L.aux, f, rot, L.player, &S, 0);
  for (int y = 0; y < 56; y++) for (int x = 0; x < 56; x++) out[y * 56 + x] = bits[(y + 8) * 64 + x + 4] ? lit : dark;
}
// ---- the shape in 3D: tiles with their rooms, walls as brick blocks, boxes as crates, the player standing up ------
static Vec3 vadd(Vec3 a, Vec3 b) { return v3f(a.x + b.x, a.y + b.y, a.z + b.z); }
static Vec3 vsc(Vec3 a, float k) { return v3f(a.x * k, a.y * k, a.z * k); }
static float light(Vec3 n) {                  // one light, up and to the side, plus some ambient
  const float lx = 0.38f, ly = 0.52f, lz = 0.76f;
  float d = n.x * lx + n.y * ly + n.z * lz;
  return 0.42f + 0.58f * (d > 0 ? d : 0);
}
static uint32_t brickPx[49], boxPx[49], boxOnPx[49], manPx[49], manTopPx[49];
static void tex_init(void) {
  static int done; if (done) return; done = 1;
  for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) {
    int brick = !(j == 3 || j == 6 || (j < 3 && i == 6) || (j > 3 && j < 6 && i == 2));
    brickPx[j * 7 + i] = brick ? VMX_LIT : 0x1d4a10;
    boxPx[j * 7 + i] = VMX_BOX[j][i] == '#' ? 0xe0c070 : 0x3a2c14;
    boxOnPx[j * 7 + i] = VMX_BOXON[j][i] == '#' ? 0xffe9a0 : 0x5a4420;
    manPx[j * 7 + i] = VMX_MAN[j][i] == '#' ? 0xffffff : 0x203040;
    manTopPx[j * 7 + i] = 0xc8d8ff;
  }
}
// a block standing on a room cell: base corner o, edges du dv (across the cell), up n, height h. skip[k]: don't draw
// side k (0 the du edge at the cell's top, 1 the right side, 2 the bottom, 3 the left), hidden by a neighbour.
static void block(Vec3 o, Vec3 du, Vec3 dv, Vec3 n, float h, const Tex *top, const Tex *side, const int *skip) {
  Vec3 b[4] = { o, vadd(o, du), vadd(vadd(o, du), dv), vadd(o, dv) }, t[4], up = vsc(n, h);
  for (int k = 0; k < 4; k++) t[k] = vadd(b[k], up);
  gfx_quad(t, top, 0xf2f6ff, light(n));
  for (int k = 0; k < 4; k++) {
    if (skip && skip[k]) continue;
    Vec3 out = k == 0 ? vsc(dv, -1) : k == 1 ? du : k == 2 ? dv : vsc(du, -1);   // outward (light only needs the way)
    float ol = sqrtf(out.x * out.x + out.y * out.y + out.z * out.z); out = vsc(out, 1 / ol);
    Vec3 Q[4] = { t[k], t[(k + 1) & 3], b[(k + 1) & 3], b[k] };   // texture's top along the block's top
    gfx_quad(Q, side, 0xf2f6ff, light(out) * 0.85f);
  }
}
// Tiles are drawn inset by GAP from every edge, and every joined edge gets a strip between the two tiles: flat between
// flat tiles, a 45-degree ramp at an inside corner, a bevel at an outside one. Walls stand GAP high, so at an inside
// corner one room's edge bricks end exactly where the other room's floor begins: nothing hangs over it. (GAP = wall
// height = (1 - 2 GAP) / 8 x 0.55, i.e. about half a cell.)
#define GAP 0.0604f
#define CS ((1.0f - 2 * GAP) / N)
typedef struct { Vec3 o, u, v, n; } FFrame;   // a side's frame (front, or the back's mirror)
static FFrame face_frame(int f) {
  const FoldSquare *q = &L.sq[f >> 1];
  FFrame F = { v3f(q->o[0], q->o[1], q->o[2]), v3f(q->u[0], q->u[1], q->u[2]), v3f(q->v[0], q->v[1], q->v[2]), v3f(q->n[0], q->n[1], q->n[2]) };
  if (f & 1) { F.o = vadd(F.o, F.u); F.u = vsc(F.u, -1); F.n = vsc(F.n, -1); }
  return F;
}
static Vec3 inset_corner(const FFrame *F, int k) {   // corner k (TL TR BR BL) of the inset tile
  float a = (k == 1 || k == 2) ? 1 - GAP : GAP, b = (k >= 2) ? 1 - GAP : GAP;
  return vadd(F->o, vadd(vsc(F->u, a), vsc(F->v, b)));
}
// a limb: a thin square rod from a to b, flat coloured
static void rod(Vec3 a, Vec3 b, float w, uint32_t col) {
  Vec3 d = vadd(b, vsc(a, -1)), ref = fabsf(d.z) < 0.9f * sqrtf(d.x * d.x + d.y * d.y + d.z * d.z) ? v3f(0, 0, 1) : v3f(1, 0, 0);
  Vec3 p1 = v3f(d.y * ref.z - d.z * ref.y, d.z * ref.x - d.x * ref.z, d.x * ref.y - d.y * ref.x);
  float l1 = sqrtf(p1.x * p1.x + p1.y * p1.y + p1.z * p1.z); p1 = vsc(p1, w / 2 / l1);
  Vec3 p2 = v3f(d.y * p1.z - d.z * p1.y, d.z * p1.x - d.x * p1.z, d.x * p1.y - d.y * p1.x);
  float l2 = sqrtf(p2.x * p2.x + p2.y * p2.y + p2.z * p2.z); p2 = vsc(p2, w / 2 / l2);
  Vec3 off[4] = { vadd(p1, p2), vadd(p1, vsc(p2, -1)), vsc(vadd(p1, p2), -1), vadd(vsc(p1, -1), p2) };
  for (int k = 0; k < 4; k++) {
    Vec3 o0 = off[k], o1 = off[(k + 1) & 3], mid = vadd(o0, o1);
    float ml = sqrtf(mid.x * mid.x + mid.y * mid.y + mid.z * mid.z);
    Vec3 Q[4] = { vadd(a, o0), vadd(a, o1), vadd(b, o1), vadd(b, o0) };
    gfx_quad(Q, NULL, col, light(vsc(mid, 1 / ml)));
  }
}
// the player as a 3D stick man standing on a cell: corner p, cell edges du dv, up n (unit), cell size cs. He turns
// round his up axis to face the camera, so his arms and legs always show.
static void stick_man(Vec3 p, Vec3 du, Vec3 dv, Vec3 n, float cs) {
  Vec3 base = vadd(p, vsc(vadd(du, dv), 0.5f)), view = vadd(eye, vsc(base, -1));
  Vec3 side = v3f(view.y * n.z - view.z * n.y, view.z * n.x - view.x * n.z, view.x * n.y - view.y * n.x);   // view x up
  float sl = sqrtf(side.x * side.x + side.y * side.y + side.z * side.z);
  side = sl > 1e-5f ? vsc(side, cs / sl) : du;
  #define AT(x, z) vadd(base, vadd(vsc(side, x), vsc(n, (z) * cs)))
  const uint32_t col = 0xf2f6ff; const float w = cs * 0.16f;
  Vec3 hip = AT(0, 0.72f), neck = AT(0, 1.3f), sh = AT(0, 1.18f);
  rod(AT(-0.3f, 0), hip, w, col); rod(AT(0.3f, 0), hip, w, col);                     // legs
  rod(hip, neck, w, col);                                                             // body
  rod(sh, AT(-0.42f, 0.78f), w, col); rod(sh, AT(0.42f, 0.78f), w, col);              // arms
  float h = 0.21f; Vec3 sd = vsc(side, h), fw = v3f(side.y * n.z - side.z * n.y, side.z * n.x - side.x * n.z, side.x * n.y - side.y * n.x);
  fw = vsc(fw, h);                                                                    // head: a little cube
  Vec3 hc = AT(0, 1.5f);
  block(vadd(hc, vadd(vsc(sd, -1), vadd(vsc(fw, -1), vsc(n, -h * cs)))), vsc(sd, 2), vsc(fw, 2), n, 2 * h * cs, NULL, NULL, NULL);
  #undef AT
}
static void room_3d(int f) {                  // the contents of side f, standing out of the tile
  FFrame F = face_frame(f);
  Vec3 o = inset_corner(&F, 0), n = F.n;
  const float cs = CS;
  Vec3 du = vsc(F.u, cs), dv = vsc(F.v, cs);
  static uint32_t steelPx[1] = { 0x9fb4c8 }, goldPx[1] = { 0xffc83c }, lockPx[49]; static int lk;
  if (!lk) { lk = 1; for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) { int hole = (j == 2 && i == 3) || (j == 3 && i >= 2 && i <= 4) || (j >= 4 && j <= 5 && i == 3); lockPx[j * 7 + i] = hole ? 0x2a1c04 : 0xffc83c; } }
  Tex gold = { 1, 1, goldPx }, lockTx = { 7, 7, lockPx };
  Tex brick = { 7, 7, brickPx }, bx = { 7, 7, boxPx }, bxOn = { 7, 7, boxOnPx }, steel = { 1, 1, steelPx };
  for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
    int c = f * NN + y * N + x;
    Vec3 p = vadd(o, vadd(vsc(du, (float)x), vsc(dv, (float)y)));
    if (L.wall[c]) {
      int skip[4] = { y > 0 && L.wall[c - N], x < N - 1 && L.wall[c + 1], y < N - 1 && L.wall[c + N], x > 0 && L.wall[c - 1] };
      block(p, du, dv, n, GAP, &brick, &brick, skip);
    } else if (L.kind[c] == FK_LOCK) {         // a lock: a gold block with a keyhole
      block(p, du, dv, n, GAP, &lockTx, &lockTx, NULL);
    } else if (L.kind[c] == FK_KEY && !L.box[c] && c != L.player) {   // a key: a gold ring and stem standing up
      block(vadd(p, vadd(vsc(du, 0.18f), vsc(dv, 0.35f))), vsc(du, 0.3f), vsc(dv, 0.3f), n, GAP * 0.5f, &gold, &gold, NULL);
      block(vadd(p, vadd(vsc(du, 0.48f), vsc(dv, 0.44f))), vsc(du, 0.36f), vsc(dv, 0.12f), n, GAP * 0.5f, &gold, &gold, NULL);
      block(vadd(p, vadd(vsc(du, 0.7f), vsc(dv, 0.56f))), vsc(du, 0.08f), vsc(dv, 0.12f), n, GAP * 0.5f, &gold, &gold, NULL);
    } else if (L.kind[c] == FK_GATE && !(L.aux[c] & 0x80)) {   // a shut gate: a fence of bars
      for (int k = 0; k < 4; k++) block(vadd(p, vadd(vsc(du, 0.12f + 0.22f * k), vsc(dv, 0.4f))), vsc(du, 0.1f), vsc(dv, 0.2f), n, GAP * 1.2f, &steel, &steel, NULL);
      block(vadd(p, vadd(vsc(du, 0.05f), vsc(dv, 0.38f))), vsc(du, 0.9f), vsc(dv, 0.24f), vsc(n, 1), GAP * 0.18f, &steel, &steel, NULL);
      if (L.box[c]) block(vadd(p, vadd(vsc(du, 0.1f), vsc(dv, 0.1f))), vsc(du, 0.8f), vsc(dv, 0.8f), n, cs * 0.75f, L.target[c] ? &bxOn : &bx, L.target[c] ? &bxOn : &bx, NULL);
      if (c == L.player) stick_man(p, du, dv, n, cs);
    } else if (L.box[c]) {
      block(vadd(p, vadd(vsc(du, 0.1f), vsc(dv, 0.1f))), vsc(du, 0.8f), vsc(dv, 0.8f), n, cs * 0.75f, L.target[c] ? &bxOn : &bx, L.target[c] ? &bxOn : &bx, NULL);
    } else if (c == L.player) {
      stick_man(p, du, dv, n, cs);
    }
  }
}
// the strips from square i's edges to the squares it joins (each pair once: i the lower index), cell by cell: floor
// where both rooms' edge cells are open (a doorway carries on across), darker where either side has bricks
static void strips(int i) {
  int f = sq_face_toward_eye(i);
  for (int e = 0; e < 4; e++) {
    FoldJoin a = S.adj[f][e];
    if (a.f < 0 || (a.f >> 1) < i) continue;
    FFrame A = face_frame(f), B = face_frame(a.f);
    Vec3 pa = inset_corner(&A, e), qa = inset_corner(&A, e + 1), pb = inset_corner(&B, a.e + 1), qb = inset_corner(&B, a.e);
    Vec3 nrm = vadd(A.n, B.n); float nl = sqrtf(nrm.x * nrm.x + nrm.y * nrm.y + nrm.z * nrm.z);
    nrm = nl > 0.01f ? vsc(nrm, 1 / nl) : A.n;
    for (int k = 0; k < N; k++) {
      float t0 = GAP / (1 - 0) + 0, t1;                     // along the edge, cell k of the inset tile
      t0 = (float)k / N; t1 = (float)(k + 1) / N;
      Vec3 a0 = vadd(pa, vsc(vadd(qa, vsc(pa, -1)), t0)), a1 = vadd(pa, vsc(vadd(qa, vsc(pa, -1)), t1));
      Vec3 b0 = vadd(pb, vsc(vadd(qb, vsc(pb, -1)), t0)), b1 = vadd(pb, vsc(vadd(qb, vsc(pb, -1)), t1));
      int p = k, x = e == 0 ? p : e == 1 ? N - 1 : e == 2 ? N - 1 - p : 0, y = e == 0 ? 0 : e == 1 ? p : e == 2 ? N - 1 : N - 1 - p;
      int c = f * NN + y * N + x, d1, c2 = fold_step(&S, c, e, &d1);
      int open = !L.wall[c] && c2 >= 0 && !L.wall[c2] && (sides[f >> 1] >> (f & 1) & 1);
      Vec3 Q[4] = { a0, a1, b1, b0 };
      gfx_quad(Q, NULL, open ? 0x0d1a0a : 0x1d4a10, light(nrm));
    }
    gfx_line3(pa, pb, C_EDGE, 1); gfx_line3(qa, qb, C_EDGE, 1);
  }
}
static void draw_shape(void) {
  gfx_zclear(); tex_init();
  static uint32_t back[8 * 8]; static int init;
  if (!init) { init = 1; for (int i = 0; i < 64; i++) back[i] = ((i % 8) + (i / 8)) & 1 ? 0x24301f : 0x141a12; }
  Tex backTex = { 8, 8, back };
  for (int i = 0; i < L.nsq; i++) {
    const FoldSquare *q = &L.sq[i];
    Vec3 o = v3f(q->o[0], q->o[1], q->o[2]), u = v3f(q->u[0], q->u[1], q->u[2]), v = v3f(q->v[0], q->v[1], q->v[2]);
    int f = sq_face_toward_eye(i), room = sides[i] >> (f & 1) & 1;
    FFrame FF = face_frame(f);
    Vec3 P[4] = { inset_corner(&FF, 0), inset_corner(&FF, 1), inset_corner(&FF, 2), inset_corner(&FF, 3) };
    Vec3 nf = FF.n;
    if (room) {
      memset(bits, 0, sizeof bits);
      vmx_room(bits, L.wall, L.target, L.box, L.btn, L.kind, L.aux, f, 0, -1, &S, VMX_FLOOR_ONLY);   // floor, arrows, targets, buttons
      for (int y = 0; y < 56; y++) for (int x = 0; x < 56; x++) {
        int k = L.kind[f * NN + (y / 7) * N + x / 7], lit = bits[(y + 8) * 64 + x + 4];
        texbuf[0][y * 56 + x] = k == FK_ICE ? (lit ? 0xe0fbff : 0x2f6f7f) :          // ice: pale blue
                                k == FK_ONEWAY ? (lit ? 0xffd040 : 0x0d1a0a) :        // one-way: a yellow arrow
                                k == FK_PIT ? (lit ? 0x1a2a14 : 0x000000) :           // pit: a black hole
                                k == FK_TELE ? (lit ? 0xe070ff : 0x200a2a) :          // teleporter: violet
                                k == FK_PLATE ? (lit ? 0xff9a3c : 0x2a1606) :         // plate: orange
                                k == FK_GATE ? (lit ? 0x9fb4c8 : 0x10161c) :          // gate: steel
                                k == FK_KEY ? (lit ? 0x4fa82a : 0x0d1a0a) :           // key: stands up (below)
                                k == FK_LOCK ? (lit ? 0xffd040 : 0x0d1a0a) :          // lock: under its block
                                k == FK_GRATE ? (lit ? 0xa0a8a0 : 0x0d1a0a) :         // grate: grey
                                lit ? 0x4fa82a : 0x0d1a0a;
      }
      Tex tx = { 56, 56, texbuf[0] }; gfx_quad(P, &tx, 0, light(nf));
    } else gfx_quad(P, &backTex, 0, light(nf));
    for (int k = 0; k < 4; k++) gfx_line3(P[k], P[(k + 1) & 3], C_EDGE, 1);
    for (int s = 0; s < 2; s++) if (sides[i] >> s & 1) room_3d(2 * i + s);
    strips(i);
    if (func[i].kind) {                       // a tile with a room move: a mark at its middle
      Vec3 c = vadd(o, vsc(vadd(u, v), 0.5f)); float sx, sy;
      if (gfx_project(c, &sx, &sy)) { char m[4] = { (char)(func[i].kind - 32), 0 }; gfx_rect((int)sx - 6, (int)sy - 6, 13, 13, 0x000000); gfx_text((int)sx - 4, (int)sy - 4, 2, 0xffd040, m); }
    }
  }
  // the working plane's grid round the cursor, and the cursor
  Vec3 o = v3f(co[0], co[1], co[2]), a = v3f(ce1[0], ce1[1], ce1[2]), b = v3f(ce2[0], ce2[1], ce2[2]);
  const int R = 4;
  for (int k = -R; k <= R + 1; k++) {
    Vec3 p0 = v3f(o.x + a.x * k - b.x * R, o.y + a.y * k - b.y * R, o.z + a.z * k - b.z * R), p1 = v3f(p0.x + b.x * (2 * R + 1), p0.y + b.y * (2 * R + 1), p0.z + b.z * (2 * R + 1));
    Vec3 q0 = v3f(o.x + b.x * k - a.x * R, o.y + b.y * k - a.y * R, o.z + b.z * k - a.z * R), q1 = v3f(q0.x + a.x * (2 * R + 1), q0.y + a.y * (2 * R + 1), q0.z + a.z * (2 * R + 1));
    gfx_line3(p0, p1, C_GRID, 1); gfx_line3(q0, q1, C_GRID, 1);
  }
  Vec3 C[4] = { o, v3f(o.x + a.x, o.y + a.y, o.z + a.z), v3f(o.x + a.x + b.x, o.y + a.y + b.y, o.z + a.z + b.z), v3f(o.x + b.x, o.y + b.y, o.z + b.z) };
  uint32_t cc = (t >> 3) & 1 ? C_CURSOR : 0xffffff;
  for (int k = 0; k < 4; k++) { float x0, y0, x1, y1; if (gfx_project(C[k], &x0, &y0) && gfx_project(C[(k + 1) & 3], &x1, &y1)) for (int w = -1; w <= 1; w++) { gfx_line((int)x0 + w, (int)y0, (int)x1 + w, (int)y1, cc); gfx_line((int)x0, (int)y0 + w, (int)x1, (int)y1 + w, cc); } }
}
static void draw_vmx(int x, int y, int scale, const uint8_t *b) {   // the VMX window: a body with its 64x64 screen
  int w = 64 * scale;
  gfx_rect(x - 14, y - 14, w + 28, w + 46, 0x2a2d2a); gfx_frame(x - 14, y - 14, w + 28, w + 46, 0x4a4f48);
  gfx_rect(x - 3, y - 3, w + 6, w + 6, 0x000000);
  static uint32_t col[64 * 64]; vmx_colours(b, col, VMX_LIT, VMX_DARK);
  Tex tx = { 64, 64, col }; gfx_blit(x, y, scale, &tx);
  gfx_text(x + w / 2 - gfx_text_w(2, "VMX") / 2, y + w + 12, 2, 0x8c9787, "VMX");
}
static void draw_room_editor(void) {
  memset(bits, 0, sizeof bits);
  vmx_room(bits, L.wall, L.target, L.box, L.btn, L.kind, L.aux, rf, rrot, L.player, &S, 0);
  const int S7 = 7, X0 = 24, Y0 = 56;          // the room 7x bigger: a cell is 49 px
  for (int y = 0; y < 56; y++) for (int x = 0; x < 56; x++) gfx_rect(X0 + x * S7, Y0 + y * S7, S7, S7, bits[(y + 8) * 64 + x + 4] ? VMX_LIT : VMX_DARK);
  uint32_t cc = (t >> 3) & 1 ? C_CURSOR : 0xffffff;
  for (int w = 0; w < 3; w++) gfx_frame(X0 + rx * 49 - w, Y0 + ry * 49 - w, 49 + 2 * w, 49 + 2 * w, cc);
  char s[64];
  snprintf(s, sizeof s, "CELL %d %s", rf >> 1, rf & 1 ? "BACK" : "FRONT"); gfx_text(X0, 30, 2, C_TEXT, s);
  int px0 = 440;
  gfx_text(px0, 56, 2, C_DIM, "TILES  L/R");
  for (int k = 0; k < NITEMS; k++) gfx_text(px0, 80 + k * 18, 2, k == item ? C_HI : C_TEXT, ITEMS[k]);
  int hy = 92 + NITEMS * 18;
  gfx_text(px0, hy, 2, C_DIM, "A PLACE"); gfx_text(px0, hy + 18, 2, C_DIM, "B FLOOR");
  gfx_text(px0, hy + 36, 2, C_DIM, "DPAD NEXT CELL"); gfx_text(px0, hy + 54, 2, C_DIM, "Y CELL"); gfx_text(px0, hy + 72, 2, C_DIM, "X LEVEL");
}
void app_draw(void) {
  gfx_clear(C_BG);
  char s[96];
  if (mode == M_PLAY) {
    static uint8_t b[64 * 64];
    vmx_game(b, &G, 1, popT ? popWord : NULL);
    if (wonT) { for (int y = 24; y < 40; y++) for (int x = 6; x < 58; x++) b[y * 64 + x] = 0; vmx_text(b, 20, 26, "CLEAR!"); snprintf(s, sizeof s, "%d MOVES", G.moves); vmx_text(b, 32 - 2 * (int)strlen(s) + 1, 33, s); }
    draw_vmx(GW / 2 - 160, 52, 5, b);
    gfx_text(20, 20, 2, C_TEXT, "PLAYTEST   DPAD/STICK MOVE   A UNDO   B BACK");
    return;
  }
  if (mode == M_ROOM) draw_room_editor();
  else {
    draw_shape();
    gfx_text(16, 14, 2, C_TEXT, "LEVEL   STICK MOVE  A PLACE CELL  B REMOVE  DPAD FOLD  L/R TURN");
    gfx_text(16, 32, 2, C_DIM, "X EDIT CELL   Y CELL SETTINGS   START MENU");
    snprintf(s, sizeof s, "%s   %d CELLS", L.name, L.nsq); gfx_text(16, GH - 22, 2, C_DIM, s);
    if (vmxWin) {                             // the VMX window: the room under the cursor, as on the handheld
      static uint8_t b[64 * 64]; memset(b, 0, sizeof b);
      int i = find_sq(co, ce1, ce2);
      if (i >= 0) { int f = sq_face_toward_eye(i); if (sides[i] >> (f & 1) & 1) vmx_room(b, L.wall, L.target, L.box, L.btn, L.kind, L.aux, f, 0, L.player, &S, 0); else vmx_text(b, 12, 28, "NO ROOM"); }
      else vmx_text(b, 4, 28, "EMPTY SPACE");
      draw_vmx(GW - 64 * 2 - 30, GH - 64 * 2 - 52, 2, b);
    }
  }
  if (mode == M_PROPS) {
    int i = propSq, n = nbuttons();
    gfx_rect(120, 120, 400, 210, 0x000000); gfx_frame(120, 120, 400, 210, C_HI);
    snprintf(s, sizeof s, "CELL %d SETTINGS", i); gfx_text(140, 136, 2, C_HI, s);
    const char *rows[4]; char r1[64], r2[64], r3[64];
    snprintf(r1, sizeof r1, "MOVE: %s", FUNCS[func_index(&func[i])]);
    if (trig[i] > n) trig[i] = 0;
    if (trig[i]) snprintf(r2, sizeof r2, "BUTTON: %d OF %d", trig[i], n); else snprintf(r2, sizeof r2, "BUTTON: NONE (%d PLACED)", n);
    snprintf(r3, sizeof r3, "BACK SIDE: %s", sides[i] & 2 ? "ON" : "OFF");
    rows[0] = r1; rows[1] = r2; rows[2] = r3; rows[3] = "DELETE CELL (A)";
    for (int k = 0; k < 4; k++) gfx_text(150, 172 + k * 26, 2, k == prow ? C_HI : C_TEXT, rows[k]);
    gfx_text(140, 300, 2, C_DIM, "UP/DOWN PICK   LEFT/RIGHT CHANGE   B DONE");
  }
  if (mode == M_MENU) {
    gfx_rect(170, 100, 300, 250, 0x000000); gfx_frame(170, 100, 300, 250, C_HI);
    gfx_text(190, 116, 3, C_HI, "MENU");
    for (int k = 0; k < 6; k++) {
      char row[96];
      if (k == 1) snprintf(row, sizeof row, "VMX WINDOW: %s", vmxWin ? "ON" : "OFF");
      else if (k == 3) snprintf(row, sizeof row, "LOAD: %s", nfiles ? files[fileIx] : "-");
      else snprintf(row, sizeof row, "%s", MENU[k]);
      gfx_text(196, 160 + k * 28, 2, k == mrow ? C_HI : C_TEXT, row);
    }
  }
  if (msgT) { int w = gfx_text_w(2, msg); gfx_rect(GW / 2 - w / 2 - 8, GH - 60, w + 16, 22, 0x000000); gfx_text(GW / 2 - w / 2, GH - 56, 2, C_HI, msg); }
}
