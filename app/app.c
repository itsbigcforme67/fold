// FOLD app: the level editor (shape view, room editor, tile settings), playtesting in the VMX window, and the menu.
// Controls (the owner's design, 2026-10-20):
//   shape view: left stick moves the cursor cell on the working plane; A places a tile (facing you), B removes it;
//     the D-pad folds the working plane 90 degrees about the cursor cell's edge that way, wrapping away from you (round
//     the shape); the cursor moves onto the new plane and the view turns to face it; L / R turn the view 90 degrees, the right stick turns / tilts it freely; X opens the
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
static const char *ITEMS[] = { "WALL", "FLOOR", "BOX", "TARGET", "BUTTON", "PLAYER" };
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
    L.wall[c] = x == 0 || y == 0 || x == N - 1 || y == N - 1; L.target[c] = L.box[c] = L.btn[c] = 0;
  }
  sides[f >> 1] |= 1 << (f & 1);
}
static void clear_face(int f) { for (int c = f * NN; c < (f + 1) * NN; c++) { L.wall[c] = 1; L.target[c] = L.box[c] = L.btn[c] = 0; } if (L.player / NN == f) L.player = -1; }
static void resurface(void) { fold_surface(&S, L.sq, L.nsq); }
static void new_level(void) {
  memset(&L, 0, sizeof L); memset(L.wall, 1, sizeof L.wall); L.player = -1;
  memset(sides, 0, sizeof sides); memset(func, 0, sizeof func); memset(trig, 0, sizeof trig);
  strcpy(L.name, "NEW SHAPE"); path[0] = 0;
  int o[3] = { 0, 0, 0 }; memcpy(co, o, sizeof co);
  ce1[0] = 1; ce1[1] = ce1[2] = 0; ce2[1] = 1; ce2[0] = ce2[2] = 0;
  resurface();
}
static int add_sq(int nc[3]) {           // a tile at the cursor, its front facing nc
  if (L.nsq == FOLD_MAXSQ) { say("TOO MANY TILES"); return -1; }
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
  uint8_t *arr[4] = { L.wall, L.target, L.box, L.btn };
  for (int k = 0; k < 4; k++) memmove(arr[k] + 2 * i * NN, arr[k] + 2 * (i + 1) * NN, (size_t)(2 * (n - i - 1) * NN));
  memmove(&L.sq[i], &L.sq[i + 1], sizeof(FoldSquare) * (n - i - 1));
  memmove(&sides[i], &sides[i + 1], (size_t)(n - i - 1)); memmove(&func[i], &func[i + 1], sizeof(FoldAct) * (n - i - 1));
  memmove(&trig[i], &trig[i + 1], sizeof(int) * (n - i - 1));
  for (int k = 0; k < n - 1; k++) if (func[k].kind && func[k].sq > i) func[k].sq--;
  if (L.player >= 0) { int f = L.player / NN; if (f >> 1 == i) L.player = -1; else if (f >> 1 > i) L.player -= 2 * NN; }
  L.nsq--;
  for (int k = 0; k < 4; k++) memset(arr[k] + 2 * L.nsq * NN, k == 0, 2 * NN);
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
      char ch = L.wall[c] ? '#' : L.box[c] ? (L.target[c] ? 'B' : B.btn[c] ? 'O' : 'b') : c == L.player ? (L.target[c] ? 'P' : 'p') : L.target[c] ? 'x' : B.btn[c] ? 'o' : '.';
      fputc(ch, f); } fputc('\n', f); }
  }
  for (int b = 0; b < B.nbtn; b++) if (B.nact[b]) {
    fprintf(f, "link %d", b + 1);
    for (int k = 0; k < B.nact[b]; k++) { const FoldAct *a = &B.act[b][k];
      if (a->kind == 's' || a->kind == 'h') fprintf(f, " %c%d:%s", a->kind, a->sq, a->spec); else fprintf(f, " %c%d", a->kind, a->sq); }
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
  for (int b = 0; b < L.nbtn; b++) for (int k = 0; k < L.nact[b]; k++) { const FoldAct *a = &L.act[b][k]; func[a->sq] = *a; trig[a->sq] = b + 1; }
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
  yawGoal += p->rx * 0.06f; pitch -= p->ry * 0.05f;
  if (pitch > 1.45f) pitch = 1.45f; if (pitch < -1.45f) pitch = -1.45f;
  int i = find_sq(co, ce1, ce2);
  if (pressed(p, PAD_A)) {
    if (i >= 0) say("A TILE IS ALREADY THERE");
    else { int nc[3]; facing_normal(nc); if (add_sq(nc) >= 0) say("TILE PLACED"); }
  }
  if (pressed(p, PAD_B) && i >= 0) { remove_sq(i); say("TILE REMOVED"); }
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
  if (corner && what != 0) { say("ROOM CORNERS STAY WALL"); return; }
  switch (what) {
    case 0: if (c == L.player) { say("MOVE THE PLAYER FIRST"); return; } L.wall[c] = 1; L.box[c] = L.target[c] = L.btn[c] = 0; break;
    case 1: L.wall[c] = 0; L.box[c] = L.target[c] = L.btn[c] = 0; break;
    case 2: if (c == L.player) return; L.wall[c] = 0; L.box[c] ^= 1; break;
    case 3: L.wall[c] = 0; L.target[c] ^= 1; break;
    case 4: L.wall[c] = 0; L.btn[c] ^= 1; break;
    case 5: if (L.box[c]) return; L.wall[c] = 0; L.player = c; break;
  }
  if (edge && !corner) {                      // a doorway opens (or closes) on both sides
    int d = y == 0 ? 0 : x == N - 1 ? 1 : y == N - 1 ? 2 : 3, d1, m = fold_step(&S, c, d, &d1);
    if (m >= 0 && (sides[(m / NN) >> 1] >> ((m / NN) & 1) & 1)) { L.wall[m] = L.wall[c]; if (L.wall[m]) { L.box[m] = L.target[m] = L.btn[m] = 0; } }
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
    if (!go_room(screen_to_cell(ex, ey), sd)) say("NO ROOM THAT WAY");
  }
  if (pressed(p, PAD_L)) item = (item + 5) % 6;
  if (pressed(p, PAD_R)) item = (item + 1) % 6;
  if (pressed(p, PAD_A)) paint(screen_to_cell(rx, ry), item);
  if (pressed(p, PAD_B)) paint(screen_to_cell(rx, ry), 1);
  if (pressed(p, PAD_X)) mode = M_SHAPE;
  if (pressed(p, PAD_Y)) { propSq = rf >> 1; prow = 0; propsBack = M_ROOM; mode = M_PROPS; }
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
  if (pressed(p, PAD_A) && prow == 3) { remove_sq(i); mode = M_SHAPE; say("TILE REMOVED"); return; }
  if (prow == 2 && !(sides[i] & 2) && propsBack == M_ROOM && rf == 2 * i + 1) propsBack = M_SHAPE;   // its room went
  if (pressed(p, PAD_B) || pressed(p, PAD_Y)) mode = propsBack;
}

// ---- menu and playtest -------------------------------------------------------------------------------------------
static const char *MENU[] = { "PLAYTEST", "VMX WINDOW", "SAVE", "LOAD", "NEW LEVEL", "QUIT" };
static char files[64][64]; static int nfiles;
static void start_play(void) {
  build(&PL);
  if (PL.player < 0) { say("PUT A PLAYER IN A ROOM FIRST"); mode = M_SHAPE; return; }
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
  vmx_room(bits, L.wall, L.target, L.box, L.btn, f, rot, L.player, &S, 0);
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
  gfx_quad(t, top, 0, light(n));
  for (int k = 0; k < 4; k++) {
    if (skip && skip[k]) continue;
    Vec3 out = k == 0 ? vsc(dv, -8) : k == 1 ? vsc(du, 8) : k == 2 ? vsc(dv, 8) : vsc(du, -8);   // unit outward
    Vec3 Q[4] = { t[k], t[(k + 1) & 3], b[(k + 1) & 3], b[k] };   // texture's top along the block's top
    gfx_quad(Q, side, 0, light(out) * 0.85f);
  }
}
static void room_3d(int f) {                  // the contents of side f, standing out of the tile
  const FoldSquare *q = &L.sq[f >> 1];
  Vec3 o = v3f(q->o[0], q->o[1], q->o[2]), u = v3f(q->u[0], q->u[1], q->u[2]), v = v3f(q->v[0], q->v[1], q->v[2]), n = v3f(q->n[0], q->n[1], q->n[2]);
  if (f & 1) { o = vadd(o, u); u = vsc(u, -1); n = vsc(n, -1); }
  const float cs = 1.0f / N;
  Vec3 du = vsc(u, cs), dv = vsc(v, cs);
  Tex brick = { 7, 7, brickPx }, bx = { 7, 7, boxPx }, bxOn = { 7, 7, boxOnPx }, man = { 7, 7, manPx }, manTop = { 7, 7, manTopPx };
  for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
    int c = f * NN + y * N + x;
    Vec3 p = vadd(o, vadd(vsc(du, (float)x), vsc(dv, (float)y)));
    if (L.wall[c]) {
      int skip[4] = { y > 0 && L.wall[c - N], x < N - 1 && L.wall[c + 1], y < N - 1 && L.wall[c + N], x > 0 && L.wall[c - 1] };
      block(p, du, dv, n, cs * 0.9f, &brick, &brick, skip);
    } else if (L.box[c]) {
      block(vadd(p, vadd(vsc(du, 0.1f), vsc(dv, 0.1f))), vsc(du, 0.8f), vsc(dv, 0.8f), n, cs * 0.75f, L.target[c] ? &bxOn : &bx, L.target[c] ? &bxOn : &bx, NULL);
    } else if (c == L.player) {
      block(vadd(p, vadd(vsc(du, 0.22f), vsc(dv, 0.22f))), vsc(du, 0.56f), vsc(dv, 0.56f), n, cs * 1.3f, &manTop, &man, NULL);
    }
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
    Vec3 P[4];
    if (!(f & 1)) { P[0] = o; P[1] = vadd(o, u); P[2] = vadd(vadd(o, u), v); P[3] = vadd(o, v); }
    else { P[0] = vadd(o, u); P[1] = o; P[2] = vadd(o, v); P[3] = vadd(vadd(o, u), v); }
    Vec3 nf = v3f(q->n[0], q->n[1], q->n[2]); if (f & 1) nf = vsc(nf, -1);
    if (room) {
      memset(bits, 0, sizeof bits);
      vmx_room(bits, L.wall, L.target, L.box, L.btn, f, 0, -1, &S, VMX_FLOOR_ONLY);   // floor, arrows, targets, buttons
      for (int y = 0; y < 56; y++) for (int x = 0; x < 56; x++) texbuf[0][y * 56 + x] = bits[(y + 8) * 64 + x + 4] ? 0x4fa82a : 0x0d1a0a;
      Tex tx = { 56, 56, texbuf[0] }; gfx_quad(P, &tx, 0, light(nf));
    } else gfx_quad(P, &backTex, 0, light(nf));
    for (int k = 0; k < 4; k++) gfx_line3(P[k], P[(k + 1) & 3], C_EDGE, 1);
    for (int s = 0; s < 2; s++) if (sides[i] >> s & 1) room_3d(2 * i + s);
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
  vmx_room(bits, L.wall, L.target, L.box, L.btn, rf, rrot, L.player, &S, 0);
  const int S7 = 7, X0 = 24, Y0 = 56;          // the room 7x bigger: a cell is 49 px
  for (int y = 0; y < 56; y++) for (int x = 0; x < 56; x++) gfx_rect(X0 + x * S7, Y0 + y * S7, S7, S7, bits[(y + 8) * 64 + x + 4] ? VMX_LIT : VMX_DARK);
  uint32_t cc = (t >> 3) & 1 ? C_CURSOR : 0xffffff;
  for (int w = 0; w < 3; w++) gfx_frame(X0 + rx * 49 - w, Y0 + ry * 49 - w, 49 + 2 * w, 49 + 2 * w, cc);
  char s[64];
  snprintf(s, sizeof s, "TILE %d %s", rf >> 1, rf & 1 ? "BACK" : "FRONT"); gfx_text(X0, 30, 2, C_TEXT, s);
  int px0 = 440;
  gfx_text(px0, 56, 2, C_DIM, "PAINT  L/R");
  for (int k = 0; k < 6; k++) gfx_text(px0, 80 + k * 18, 2, k == item ? C_HI : C_TEXT, ITEMS[k]);
  gfx_text(px0, 200, 2, C_DIM, "A PAINT");
  gfx_text(px0, 218, 2, C_DIM, "B FLOOR");
  gfx_text(px0, 236, 2, C_DIM, "DPAD ROOM");
  gfx_text(px0, 254, 2, C_DIM, "Y TILE");
  gfx_text(px0, 272, 2, C_DIM, "X SHAPE");
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
    gfx_text(16, 14, 2, C_TEXT, "SHAPE   STICK MOVE  A PLACE  B REMOVE  DPAD FOLD  L/R TURN");
    gfx_text(16, 32, 2, C_DIM, "X EDIT ROOM   Y TILE SETTINGS   START MENU");
    snprintf(s, sizeof s, "%s   %d TILES", L.name, L.nsq); gfx_text(16, GH - 22, 2, C_DIM, s);
    if (vmxWin) {                             // the VMX window: the room under the cursor, as on the handheld
      static uint8_t b[64 * 64]; memset(b, 0, sizeof b);
      int i = find_sq(co, ce1, ce2);
      if (i >= 0) { int f = sq_face_toward_eye(i); if (sides[i] >> (f & 1) & 1) vmx_room(b, L.wall, L.target, L.box, L.btn, f, 0, L.player, &S, 0); else vmx_text(b, 12, 28, "NO ROOM"); }
      else vmx_text(b, 8, 28, "EMPTY CELL");
      draw_vmx(GW - 64 * 2 - 30, GH - 64 * 2 - 52, 2, b);
    }
  }
  if (mode == M_PROPS) {
    int i = propSq, n = nbuttons();
    gfx_rect(120, 120, 400, 210, 0x000000); gfx_frame(120, 120, 400, 210, C_HI);
    snprintf(s, sizeof s, "TILE %d SETTINGS", i); gfx_text(140, 136, 2, C_HI, s);
    const char *rows[4]; char r1[64], r2[64], r3[64];
    snprintf(r1, sizeof r1, "MOVE: %s", FUNCS[func_index(&func[i])]);
    if (trig[i] > n) trig[i] = 0;
    if (trig[i]) snprintf(r2, sizeof r2, "BUTTON: %d OF %d", trig[i], n); else snprintf(r2, sizeof r2, "BUTTON: NONE (%d PLACED)", n);
    snprintf(r3, sizeof r3, "BACK ROOM: %s", sides[i] & 2 ? "ON" : "OFF");
    rows[0] = r1; rows[1] = r2; rows[2] = r3; rows[3] = "DELETE TILE (A)";
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
