// FOLD app: the level editor (level view, cell editor, cell settings), playtesting in the VMX window, and the menu.
// Words (owner, 2026-10-20): LEVEL = the whole level; CELL = one room square (a "square" / face pair in the core);
// TILE = one element inside a cell (a wall, floor, box, target, button, the player; a "cell" in the core's arrays).
// Controls (the owner's design, 2026-10-20):
//   shape view: left stick moves the cursor cell on the working plane; A places a tile (facing you), B removes it;
//     the D-pad folds the working plane 90 degrees about the cursor cell's edge that way, wrapping away from you (round
//     the shape); the cursor moves onto the new plane and the view turns to face it; L / R turn the view 90 degrees, the right stick turns / tilts it freely (with BACK held it zooms); X opens the
//     room on the side facing you; Y opens the tile's settings (its room move, the button that triggers it, the back
//     room); START opens the menu; clicking the right stick shows / hides the VMX in the middle of the screen.
//   room editor: left stick moves the cell cursor (off an edge it goes into the next room); the D-pad jumps to the
//     next room that way; A paints the item, B clears to floor, L / R choose the item; doorways on the outer ring are
//     mirrored into the room beyond; X goes back to the shape view.
#include "app.h"
#include "gfx.h"
#include "vmx.h"
#include "icons3d.h"
#include "../core/fold.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>   // the Xbox (nxdk): FindFirstFile
#else
#include <dirent.h>
#endif

#define N FOLD_N
#define NN FOLD_NN
#define PI 3.14159265f
enum { M_SHAPE, M_ROOM, M_PROPS, M_MENU, M_PLAY, M_TILE, M_WIRE };

// colours (TV)
#define C_BG 0x0b0e0a
#define C_GRID 0x1e3318
#define C_EDGE 0x3f7f2c
#define C_CURSOR 0xd8ff9c
#define C_TEXT 0xd9e3d2
#define C_DIM 0x8c9787
#define C_HI 0x7dff3a

// ---- the level being built ---------------------------------------------------------------------------------------
static FoldLevel L;                       // the level: cells (squares), tiles, signals (btn = the signal a tile sends)
static FoldSurface S;                     // joins for the squares where they are
static uint8_t sides[FOLD_MAXSQ];         // bit 0 front is a room, bit 1 back is a room
static FoldAct func[FOLD_MAXSQ];          // a tile's room move (kind 0 = none)
static int trig[FOLD_MAXSQ];              // the signal that works it (0 = none)
static char levelsDir[256] = "levels", path[300] = "", msg[64] = "";
static int msgT, mode = M_SHAPE, quit, t;

// cursor cell (shape view): corner co, axes ce1 ce2 (positive unit grid vectors)
static int co[3], ce1[3] = { 1, 0, 0 }, ce2[3] = { 0, 1, 0 };
// camera: orbit round the cursor
// the camera rides on the working plane: Fn its normal towards you, Fr / Fu its screen right / up (whole axes; a
// fold rolls the frame 90 degrees over the edge, so the new cell is seen just as the last one was). The view shown
// eases towards the frame (vn / vr / vu); the right stick adds a look round (orbit, pitch), L / R turn the frame.
static int Fn[3] = { 0, 0, 1 }, Fr[3] = { 1, 0, 0 }, Fu[3] = { 0, 1, 0 };
static Vec3 vn = { 0, 0, 1 }, vr = { 1, 0, 0 }, vu = { 0, 1, 0 };
static float orbit = 0, pitch = 0.9f, dist = 5.5f;
static int flipT, flipN[3], flipR[3];   // swinging round to the other side: the second quarter turn, after flipT frames
static Vec3 look = { 0.5f, 0.5f, 0 }, eye;
static Mat4 VPm;
// room editor
static int rf, rrot, rx = 3, ry = 3, item = 1;
static const char *ITEMS[] = { "WALL", "FLOOR", "BOX", "TARGET", "BUTTON", "PLAYER", "ICE", "ONE-WAY", "PIT", "TELEPORTER", "PLATE", "GATE", "KEY", "LOCK", "GRATE", "SENSOR", "WALL GATE", "LIGHT" };
#define NITEMS 18
// settings / menu
static int prow, mrow, fileIx, vmxWin = 0, propSq, propsBack = M_SHAPE, trow, tcell, wrow, wfield;
// playtest
static FoldLevel PL; static FoldGame G; static int popT, wonT, holdB; static const char *popWord;
static char talk[FOLD_TEXTLEN * 2]; static int talkOn, talkPage;   // what the tile you talked to says

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
static void camera_reset(void);
static void new_level(void) {
  memset(&L, 0, sizeof L); memset(L.wall, 1, sizeof L.wall); L.player = -1;
  memset(sides, 0, sizeof sides); memset(func, 0, sizeof func); memset(trig, 0, sizeof trig);
  strcpy(L.name, "NEW LEVEL"); path[0] = 0;
  int o[3] = { 0, 0, 0 }; memcpy(co, o, sizeof co);
  ce1[0] = 1; ce1[1] = ce1[2] = 0; ce2[1] = 1; ce2[0] = ce2[2] = 0;
  camera_reset();
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
// ---- signals -----------------------------------------------------------------------------------------------------
static int sig_used(int k) {                  // does anything send signal k?
  if (L.allSig == k) return 1;
  for (int i = 0; i < L.ntimer; i++) if (L.timer[i].sig == k) return 1;
  for (int i = 0; i < L.nlogic; i++) if (L.logic[i].out == k) return 1;
  for (int c = 0; c < 2 * L.nsq * NN; c++) if (L.btn[c] == k && !L.wall[c]) return 1;
  return 0;
}
static int next_signal(void) { for (int k = 1; k < FOLD_MAXSIG; k++) if (!sig_used(k)) return k; return 0; }
// the next signal number to offer, stepping dir from cur: the ones in use, a new one, and 0 (none) if allowed
static int sig_cycle(int cur, int dir, int allowNone) {
  int list[FOLD_MAXSIG + 1], n = 0, fresh = next_signal();
  if (allowNone) list[n++] = 0;
  for (int k = 1; k < FOLD_MAXSIG; k++) if (sig_used(k) || k == fresh || k == cur) list[n++] = k;
  int at = 0; for (int i = 0; i < n; i++) if (list[i] == cur) at = i;
  return list[(at + dir + n) % n];
}
// the level as the core wants it: the cells' moves become links on their signals, logic compacted
static void build(FoldLevel *out) {
  *out = L;
  memset(out->nact, 0, sizeof out->nact); memset(out->geo, 0, sizeof out->geo);
  for (int i = 0; i < L.nsq; i++) {
    if (!func[i].kind || trig[i] < 1 || trig[i] >= FOLD_MAXSIG || out->nact[trig[i] - 1] == FOLD_MAXACT) continue;
    FoldAct a = func[i]; a.sq = (uint8_t)i;
    out->act[trig[i] - 1][out->nact[trig[i] - 1]++] = a;
    if ((a.kind == 's' || a.kind == 'h') && !out->geo[i]) { out->geo[i] = 1; out->geoAct[i] = a; }
  }
  out->nlogic = 0;
  for (int k = 0; k < L.nlogic; k++) {        // empty input slots (0) are left out; a piece with no inputs is dropped
    FoldLogic q = L.logic[k]; q.n = 0;
    for (int i = 0; i < 4; i++) if (L.logic[k].in[i]) q.in[q.n++] = L.logic[k].in[i];
    if (q.n && q.out) out->logic[out->nlogic++] = q;
  }
  for (int c = 0; c < 2 * L.nsq * NN; c++) if (L.wall[c]) { out->btn[c] = out->drv[c] = 0; }
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
      char ch = L.kind[c] == FK_ICE ? '~' : L.kind[c] == FK_ONEWAY ? "^>v<"[L.aux[c] & 3] : L.kind[c] == FK_PIT ? '_' : L.kind[c] == FK_TELE ? (char)('0' + L.aux[c]) : L.kind[c] == FK_PLATE ? '=' : L.kind[c] == FK_GATE ? (L.aux[c] & 0x40 ? (L.aux[c] & 0x80 ? ';' : '%') : L.aux[c] & 0x80 ? ':' : '|') : L.kind[c] == FK_SENSOR ? '*' : L.kind[c] == FK_LIGHT ? (L.aux[c] & 0x80 ? '$' : '@') : L.kind[c] == FK_KEY ? 'k' : L.kind[c] == FK_LOCK ? 'l' : L.kind[c] == FK_GRATE ? '+' :   // special floors: a box /
        // the player on them goes on its own line
        L.wall[c] ? '#' : L.box[c] ? (L.target[c] ? 'B' : B.btn[c] ? 'O' : 'b') : c == L.player ? (L.target[c] ? 'P' : 'p') : L.target[c] ? 'x' : B.btn[c] ? 'o' : '.';
      fputc(ch, f); } fputc('\n', f); }
    for (int c = fc * NN; c < (fc + 1) * NN; c++) if (L.kind[c] && !L.wall[c]) {
      if (L.box[c]) fprintf(f, "box %d %c %d %d\n", fc >> 1, fc & 1 ? 'b' : 'f', c % N, (c / N) % N);
      if (c == L.player) fprintf(f, "player %d %c %d %d\n", fc >> 1, fc & 1 ? 'b' : 'f', c % N, (c / N) % N);
    }
    for (int c = fc * NN; c < (fc + 1) * NN; c++) if (!L.wall[c]) {   // signals sent / taken, and tiles' own text
      char sd = fc & 1 ? 'b' : 'f';
      if (B.btn[c]) fprintf(f, "signal %d %c %d %d %d\n", fc >> 1, sd, c % N, (c / N) % N, B.btn[c]);
      if (B.drv[c]) fprintf(f, "drive %d %c %d %d %d\n", fc >> 1, sd, c % N, (c / N) % N, B.drv[c]);
      if (L.txt[c]) fprintf(f, "text %d %c %d %d %s\n", fc >> 1, sd, c % N, (c / N) % N, L.text[L.txt[c] - 1]);
    }
  }
  if (B.allSig) fprintf(f, "input %d alltargets\n", B.allSig);
  for (int k = 0; k < B.ntimer; k++) if (B.timer[k].sig) fprintf(f, "input %d %s %d\n", B.timer[k].sig, B.timer[k].kind == FT_TICK ? "tick" : "moves", B.timer[k].n ? B.timer[k].n : 1);
  static const char *OPS[] = { "and", "or", "xor", "not", "latch", "count", "delay" };
  for (int k = 0; k < B.nlogic; k++) {
    const FoldLogic *q = &B.logic[k];
    fprintf(f, "logic %d %s", q->out, OPS[q->op]);
    if (q->op == FL_COUNT || q->op == FL_DELAY) fprintf(f, " %d", q->count ? q->count : 1);
    for (int i = 0; i < q->n; i++) fprintf(f, " %d", q->in[i]);
    fputc('\n', f);
  }
  for (int b = 0; b < FOLD_MAXSIG - 1; b++) if (B.nact[b]) {
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
  for (int b = 0; b < FOLD_MAXSIG - 1; b++) for (int k = 0; k < L.nact[b]; k++) { const FoldAct *a = &L.act[b][k]; func[a->sq] = *a; trig[a->sq] = b + 1; }
  for (int k = 0; k < L.nlogic; k++) for (int i = L.logic[k].n; i < 4; i++) L.logic[k].in[i] = 0;
  // cursor on square 0, flat in its plane
  const FoldSquare *q = &L.sq[0];
  for (int i = 0; i < 3; i++) { co[i] = q->o[i] + (q->u[i] < 0 ? q->u[i] : 0) + (q->v[i] < 0 ? q->v[i] : 0); ce1[i] = abs(q->u[i]); ce2[i] = abs(q->v[i]); }
  resurface();
  return 1;
}
static int list_levels(char names[][64], int max) {
  int n = 0;
#ifdef _WIN32
  char pat[300]; snprintf(pat, sizeof pat, "%s\\*.fold", levelsDir);
  WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd); if (h == INVALID_HANDLE_VALUE) return 0;
  do { if (n < max) snprintf(names[n++], 64, "%s", fd.cFileName); } while (FindNextFileA(h, &fd));
  FindClose(h);
#else
  DIR *d = opendir(levelsDir); if (!d) return 0;
  struct dirent *e;
  while ((e = readdir(d)) && n < max) { size_t l = strlen(e->d_name); if (l > 5 && !strcmp(e->d_name + l - 5, ".fold")) snprintf(names[n++], 64, "%s", e->d_name); }
  closedir(d);
#endif
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
static int stickT, rstickT;
static int rstick_rep(float x, float y, int *dir) {   // the right stick, the same way (its own repeat)
  float ax = fabsf(x), ay = fabsf(y);
  if (ax < 0.5f && ay < 0.5f) { rstickT = 0; return 0; }
  rstickT++;
  if (rstickT != 1 && !(rstickT > 8 && rstickT % 4 == 0)) return 0;
  *dir = ax > ay ? (x > 0 ? 3 : 2) : (y > 0 ? 1 : 0);
  return 1;
}
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
static Vec3 iv(const int *a) { return v3f((float)a[0], (float)a[1], (float)a[2]); }
static Vec3 vnormz(Vec3 a) { float l = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z); return l > 1e-6f ? v3f(a.x / l, a.y / l, a.z / l) : a; }
static float vdot3(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec3 vcross3(Vec3 a, Vec3 b) { return v3f(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static Vec3 vmix(Vec3 a, Vec3 b, float k) { return v3f(a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, a.z + (b.z - a.z) * k); }
static void view_axes(Vec3 *r, Vec3 *u) {        // the shown screen right / up, with the look round
  float c = cosf(orbit), s = sinf(orbit);
  *r = v3f(vr.x * c + vu.x * s, vr.y * c + vu.y * s, vr.z * c + vu.z * s);
  *u = v3f(vu.x * c - vr.x * s, vu.y * c - vr.y * s, vu.z * c - vr.z * s);
}
static void camera_reset(void) {
  int n[3] = { 0, 0, 1 }, r[3] = { 1, 0, 0 }, u[3] = { 0, 1, 0 };
  memcpy(Fn, n, sizeof n); memcpy(Fr, r, sizeof r); memcpy(Fu, u, sizeof u);
  vn = iv(Fn); vr = iv(Fr); vu = iv(Fu); orbit = 0; pitch = 0.9f;
}
static void camera(void) {
  if (flipT && !--flipT) { memcpy(Fn, flipN, sizeof Fn); memcpy(Fr, flipR, sizeof Fr); }
  Vec3 c = cur_centre();
  look.x += (c.x - look.x) * 0.25f; look.y += (c.y - look.y) * 0.25f; look.z += (c.z - look.z) * 0.25f;
  vn = vnormz(vmix(vn, iv(Fn), 0.25f)); vr = vmix(vr, iv(Fr), 0.25f);   // ease the frame, keep it square
  vr = vnormz(v3f(vr.x - vn.x * vdot3(vr, vn), vr.y - vn.y * vdot3(vr, vn), vr.z - vn.z * vdot3(vr, vn)));
  vu = vcross3(vn, vr);
  Vec3 r, u; view_axes(&r, &u);
  float cp = cosf(pitch), sp = sinf(pitch);      // from above the plane, tipped back towards the screen's bottom
  eye = v3f(look.x + dist * (vn.x * sp - u.x * cp), look.y + dist * (vn.y * sp - u.y * cp), look.z + dist * (vn.z * sp - u.z * cp));
  Vec3 up = v3f(u.x * sp + vn.x * cp, u.y * sp + vn.y * cp, u.z * sp + vn.z * cp);
  VPm = mat_mul(mat_perspective(1.05f, (float)GW / GH, 0.05f, 100.0f), mat_lookat(eye, look, up));
  gfx_camera(VPm);
}
// screen direction sd (0 up 1 right 2 down 3 left) as one of the frame's in-plane axes: the one nearest to that
// screen direction as shown (the look round can turn the view a long way)
static void plane_dir(int sd, int out[3]) {
  Vec3 r, u; view_axes(&r, &u);
  Vec3 want = sd == 0 ? u : sd == 1 ? r : sd == 2 ? v3f(-u.x, -u.y, -u.z) : v3f(-r.x, -r.y, -r.z);
  const int *cand[2] = { Fr, Fu }; float best = -1e9f;
  for (int k = 0; k < 4; k++) {
    int sg = k & 1 ? -1 : 1; const int *a = cand[k >> 1];
    float d = sg * (a[0] * want.x + a[1] * want.y + a[2] * want.z);
    if (d > best) { best = d; for (int i = 0; i < 3; i++) out[i] = sg * a[i]; }
  }
}
static void facing_normal(int nc[3]) { memcpy(nc, Fn, sizeof Fn); }   // the cursor plane's normal on your side
static int sq_face_toward_eye(int i) {   // which side of square i faces the camera
  const FoldSquare *q = &L.sq[i];
  Vec3 c = v3f(q->o[0] + (q->u[0] + q->v[0]) * 0.5f, q->o[1] + (q->u[1] + q->v[1]) * 0.5f, q->o[2] + (q->u[2] + q->v[2]) * 0.5f);
  float d = (eye.x - c.x) * q->n[0] + (eye.y - c.y) * q->n[1] + (eye.z - c.z) * q->n[2];
  return 2 * i + (d < 0);
}

// ---- shape view --------------------------------------------------------------------------------------------------
static void fold_plane(int sd) {
  // roll over the cursor cell's edge that way, round the back of the shape (away from you); the camera rolls with
  // it, so the new cell faces you as the last one did and the last one now hangs away behind its edge
  int a[3], b[3], n0[3]; plane_dir(sd, a); memcpy(n0, Fn, sizeof n0);
  int pos = a[0] + a[1] + a[2] > 0;               // folding over the cell's far edge along that axis
  const int *other = (abs(a[0]) == ce1[0] && abs(a[1]) == ce1[1] && abs(a[2]) == ce1[2]) ? ce2 : ce1;
  memcpy(b, other, sizeof b);
  int start[3], away[3];
  for (int i = 0; i < 3; i++) { start[i] = co[i] + (pos ? a[i] : 0); away[i] = -n0[i]; }   // pivot edge's low end
  for (int i = 0; i < 3; i++) { co[i] = start[i] + (away[i] < 0 ? away[i] : 0); ce1[i] = b[i]; ce2[i] = abs(away[i]); }
  // the frame: the new normal is the way you went; that screen axis now points round the back (-old normal)
  int *ax = (abs(a[0]) == abs(Fr[0]) && abs(a[1]) == abs(Fr[1]) && abs(a[2]) == abs(Fr[2])) ? Fr : Fu;
  int same = a[0] == ax[0] && a[1] == ax[1] && a[2] == ax[2];
  for (int i = 0; i < 3; i++) { Fn[i] = a[i]; ax[i] = same ? -n0[i] : n0[i]; }
}
static void flip_side(void) {                  // left stick click: round to the other side of the cell (two quarter turns)
  for (int i = 0; i < 3; i++) { flipN[i] = -Fn[i]; flipR[i] = -Fr[i]; }
  int n[3]; memcpy(n, Fn, sizeof n);
  for (int i = 0; i < 3; i++) { Fn[i] = Fr[i]; Fr[i] = -n[i]; }   // half way: looking along the plane
  flipT = 8;
}
static void turn_view(int dir) {                // L / R: a quarter turn of the frame about the plane's normal
  int r[3], u[3];
  for (int i = 0; i < 3; i++) { r[i] = dir > 0 ? -Fu[i] : Fu[i]; u[i] = dir > 0 ? Fr[i] : -Fr[i]; }
  memcpy(Fr, r, sizeof r); memcpy(Fu, u, sizeof u);
}
// the turn of side f's room in the cell editor that matches how it lies on screen in the level view: the editor
// draws the room's +x to the right and +y down at turn 0, so take the turn whose screen directions fit best
static int open_rot(int f) {
  const FoldSquare *q = &L.sq[f >> 1]; int sg = f & 1 ? -1 : 1;
  Vec3 ux = v3f(sg * q->u[0], sg * q->u[1], sg * q->u[2]), uy = v3f(q->v[0], q->v[1], q->v[2]), r, u;
  view_axes(&r, &u);
  int best = 0; float bs = -1e9f;
  for (int k = 0; k < 4; k++) {
    int x0, y0, x1, y1, x2, y2;
    fold_to_screen(0, 0, k, &x0, &y0); fold_to_screen(1, 0, k, &x1, &y1); fold_to_screen(0, 1, k, &x2, &y2);
    // the room's +x and +y as screen steps, then as directions in the 3D view (screen down = -up)
    Vec3 wx = v3f(r.x * (x1 - x0) - u.x * (y1 - y0), r.y * (x1 - x0) - u.y * (y1 - y0), r.z * (x1 - x0) - u.z * (y1 - y0));
    Vec3 wy = v3f(r.x * (x2 - x0) - u.x * (y2 - y0), r.y * (x2 - x0) - u.y * (y2 - y0), r.z * (x2 - x0) - u.z * (y2 - y0));
    float sc = vdot3(wx, ux) + vdot3(wy, uy);
    if (sc > bs) { bs = sc; best = k; }
  }
  return best;
}
static void shape_frame(const Pad *p) {
  int d;
  if (dpad_rep(p, &d)) {                 // the D-pad moves the cursor along the plane
    int a[3]; plane_dir(PADDIR[d], a);
    for (int i = 0; i < 3; i++) co[i] += a[i];
  }
  if (stick_rep(p->lx, p->ly, &d)) fold_plane(PADDIR[d]);   // the left stick rolls the plane round the shape
  if (pressed(p, PAD_L)) turn_view(-1);
  if (pressed(p, PAD_R)) turn_view(1);
  if (pressed(p, PAD_LS) && !flipT) flip_side();
  if (p->held & PAD_BACK) { dist *= 1 + p->ry * 0.04f; if (dist < 1.2f) dist = 1.2f; if (dist > 20) dist = 20; }   // BACK + right stick: zoom
  else { orbit += p->rx * 0.06f; pitch += p->ry * 0.05f; }   // right stick: look round (up / down inverted, owner)
  if (p->held & PAD_ZIN) dist /= 1.04f;                       // Black / White: zoom in / out
  if (p->held & PAD_ZOUT) dist *= 1.04f;
  if (dist < 1.2f) dist = 1.2f; if (dist > 20) dist = 20;
  if (pitch > 1.5f) pitch = 1.5f; if (pitch < 0.15f) pitch = 0.15f;
  int i = find_sq(co, ce1, ce2);
  // A and B work on the side of the cell facing you: A places a cell, or a room on an empty back (the
  // checkerboard); B clears a back room to nothing again, or removes the cell (from its front or an empty back)
  int f = i >= 0 ? sq_face_toward_eye(i) : -1, has = f >= 0 && (sides[i] >> (f & 1) & 1);
  if (pressed(p, PAD_A)) {
    if (i < 0) { int nc[3]; facing_normal(nc); if (add_sq(nc) >= 0) say("CELL PLACED"); }
    else if (!has) { make_room(f); say("ROOM ADDED ON THE BACK"); }
    else say("A CELL IS ALREADY THERE");
  }
  if (pressed(p, PAD_B) && i >= 0) {
    if ((f & 1) && has) { clear_face(f); sides[i] &= ~2; say("BACK ROOM REMOVED"); }
    else { remove_sq(i); say("CELL REMOVED"); }
  }
  if (pressed(p, PAD_Y) && i >= 0) {          // Y: into the cell editor (X and Y swapped, owner)
    if (!has) say("NO ROOM HERE: A ADDS ONE");
    else { rf = f; rrot = open_rot(rf); mode = M_ROOM; }
  }
  if (pressed(p, PAD_X) && i >= 0) { propSq = i; prow = 0; propsBack = M_SHAPE; mode = M_PROPS; }   // X: the cell's settings
  if (pressed(p, PAD_START)) { mrow = 0; mode = M_MENU; }
  if (pressed(p, PAD_RS)) vmxWin ^= 1;        // right stick click: the VMX pops up in the middle / goes away
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
    case 0: if (c == L.player) { say("MOVE THE PLAYER FIRST"); return; } L.wall[c] = 1; L.box[c] = L.target[c] = L.btn[c] = L.kind[c] = L.aux[c] = L.drv[c] = L.txt[c] = 0; break;
    case 1: L.wall[c] = 0; L.box[c] = L.target[c] = L.btn[c] = L.kind[c] = L.aux[c] = L.drv[c] = 0; break;
    case 2: if (c == L.player) return; L.wall[c] = 0; L.box[c] ^= 1; break;
    case 3: L.wall[c] = 0; L.target[c] ^= 1; break;
    case 4: L.wall[c] = 0; if (L.kind[c] == FK_PLATE || L.kind[c] == FK_SENSOR) L.kind[c] = FK_FLOOR; L.btn[c] = L.btn[c] ? 0 : (uint8_t)next_signal(); break;
    case 5: if (L.box[c]) return; L.wall[c] = 0; L.player = c; break;
    case 6: L.wall[c] = 0; L.btn[c] = 0; L.kind[c] = L.kind[c] == FK_ICE ? FK_FLOOR : FK_ICE; break;   // ice
    case 7:                                   // one-way: placed pointing up the screen; A again turns it
      L.wall[c] = 0; L.btn[c] = 0;
      if (L.kind[c] == FK_ONEWAY) L.aux[c] = (uint8_t)((L.aux[c] + 1) & 3); else { L.kind[c] = FK_ONEWAY; L.aux[c] = (uint8_t)rrot; }
      break;
    case 8: if (c == L.player) return; L.wall[c] = 0; L.btn[c] = L.box[c] = L.target[c] = 0; L.kind[c] = L.kind[c] == FK_PIT ? FK_FLOOR : FK_PIT; break;
    case 11:                                  // gate: placed shut; A again opens / shuts it (how it starts); Y picks its button
      L.wall[c] = 0; L.btn[c] = 0;
      if (L.kind[c] == FK_GATE && !(L.aux[c] & 0x40)) L.aux[c] ^= 0x80; else { L.kind[c] = FK_GATE; L.aux[c] = 0; say("X ON A GATE PICKS ITS SIGNAL"); }
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
      if (L.kind[c] == FK_PLATE) { L.kind[c] = FK_FLOOR; L.btn[c] = 0; } else { L.kind[c] = FK_PLATE; if (!L.btn[c]) L.btn[c] = (uint8_t)next_signal(); }
      break;
    case 15:                                  // sensor: on while you stand on it
      L.wall[c] = 0;
      if (L.kind[c] == FK_SENSOR) { L.kind[c] = FK_FLOOR; L.btn[c] = 0; } else { L.kind[c] = FK_SENSOR; if (!L.btn[c]) L.btn[c] = (uint8_t)next_signal(); }
      break;
    case 17:                                  // a light: lit by its signal, and sending its own while lit (Y sets both)
      if (c == L.player) return;
      L.wall[c] = 0; L.box[c] = L.target[c] = 0;
      if (L.kind[c] == FK_LIGHT) { L.kind[c] = FK_FLOOR; L.btn[c] = L.drv[c] = 0; }
      else { L.kind[c] = FK_LIGHT; L.aux[c] = 0; if (!L.btn[c]) L.btn[c] = (uint8_t)next_signal(); say("X ON IT: WHAT LIGHTS IT"); }
      break;
    case 16:                                  // a gate that looks like brick wall (a wall that comes and goes)
      L.wall[c] = 0; L.btn[c] = 0;
      if (L.kind[c] == FK_GATE && (L.aux[c] & 0x40)) L.aux[c] ^= 0x80; else { L.kind[c] = FK_GATE; L.aux[c] = 0x40; say("X ON IT PICKS ITS SIGNAL"); }
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
static void room_turn(int dir) {             // turn the room on screen, the cursor staying on its tile
  int c = screen_to_cell(rx, ry);
  rrot = (rrot + (dir > 0 ? 3 : 1)) & 3;   // R: clockwise
  fold_to_screen(c % N, (c / N) % N, rrot, &rx, &ry);
}
static void room_frame(const Pad *p) {
  int d;
  if (dpad_rep(p, &d)) {                      // the D-pad moves the cursor (off the edge: into the next cell)
    int sd = PADDIR[d], nx = rx + (int)SX[sd], ny = ry + (int)SY[sd];
    if (nx >= 0 && ny >= 0 && nx < N && ny < N) { rx = nx; ry = ny; }
    else go_room(screen_to_cell(rx, ry), sd);
  }
  if (stick_rep(p->lx, p->ly, &d)) {          // the left stick jumps a whole cell: from the edge tile that way
    int sd = PADDIR[d], ex = sd == 1 ? N - 1 : sd == 3 ? 0 : rx, ey = sd == 2 ? N - 1 : sd == 0 ? 0 : ry;
    if (!go_room(screen_to_cell(ex, ey), sd)) say("NO CELL THAT WAY");
  }
  if (pressed(p, PAD_L)) room_turn(-1);       // L / R: a quarter turn of the room on screen
  if (pressed(p, PAD_R)) room_turn(1);
  if (rstick_rep(p->rx, p->ry, &d)) {         // the right stick moves round the tile palette (4 a row)
    int sd = PADDIR[d];
    if (sd == 1) item = (item + 1) % NITEMS; else if (sd == 3) item = (item + NITEMS - 1) % NITEMS;
    else if (sd == 0 && item >= 4) item -= 4; else if (sd == 2 && item + 4 < NITEMS) item += 4;
  }
  if (pressed(p, PAD_A)) paint(screen_to_cell(rx, ry), item);
  if (pressed(p, PAD_B)) paint(screen_to_cell(rx, ry), 1);
  if (pressed(p, PAD_Y)) mode = M_SHAPE;     // Y: back to the level
  if (pressed(p, PAD_X)) {                    // X: this tile's settings
    tcell = screen_to_cell(rx, ry); trow = 0; mode = M_TILE;   // this tile's settings (from there, the cell's)
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
enum { P_MOVE, P_SIGNAL, P_TURNMODE, P_DELETE };
static int prop_rows(int i, int *rows) {      // which rows the cell settings show
  int n = 0; rows[n++] = P_MOVE; rows[n++] = P_SIGNAL;
  if (func[i].kind == 'r' || func[i].kind == 'R') rows[n++] = P_TURNMODE;
  rows[n++] = P_DELETE;
  return n;
}
static void props_frame(const Pad *p) {
  int d, i = propSq, rows[8], nr = prop_rows(i, rows);
  if (prow >= nr) prow = nr - 1;
  if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) {
    int sd = PADDIR[d];
    if (sd == 0) prow = (prow + nr - 1) % nr; else if (sd == 2) prow = (prow + 1) % nr;
    else {
      int st = sd == 1 ? 1 : -1, r = rows[prow];
      if (r == P_MOVE) {
        int k = func_index(&func[i]);
        for (int tries = 0; tries < NFUNCS; tries++) {
          k = (k + st + NFUNCS) % NFUNCS; FoldAct a = func_of(k); a.sq = (uint8_t)i;
          if (func_ok(i, &a)) { func[i] = a; break; }
        }
      } else if (r == P_SIGNAL) trig[i] = sig_cycle(trig[i], st, 1);
      else if (r == P_TURNMODE) func[i].kind = func[i].kind == 'r' ? 'R' : 'r';
    }
  }
  if (pressed(p, PAD_A) && rows[prow] == P_DELETE) { remove_sq(i); mode = M_SHAPE; say("CELL REMOVED"); return; }
  if (pressed(p, PAD_B) || pressed(p, PAD_X)) mode = propsBack;
}

// ---- tile settings (Y on a tile in the cell editor) ---------------------------------------------------------------
enum { T_SENDS, T_WORKED, T_STARTS, T_LOOKS, T_CELL };
static int sends_ok(int c) { return L.kind[c] == FK_PLATE || L.kind[c] == FK_SENSOR || L.kind[c] == FK_KEY || L.kind[c] == FK_LIGHT || L.target[c] || (L.btn[c] && L.kind[c] == FK_FLOOR); }
static int worked_ok(int c) { return L.kind[c] == FK_GATE || L.kind[c] == FK_TELE || L.kind[c] == FK_ONEWAY || L.kind[c] == FK_LIGHT; }
static int tile_rows(int c, int *rows) {
  int n = 0;
  if (sends_ok(c)) rows[n++] = T_SENDS;
  if (worked_ok(c)) rows[n++] = T_WORKED;
  if (L.kind[c] == FK_GATE) { rows[n++] = T_STARTS; rows[n++] = T_LOOKS; }
  if (L.kind[c] == FK_TELE || L.kind[c] == FK_LIGHT) rows[n++] = T_STARTS;
  rows[n++] = T_CELL;
  return n;
}
static void tile_frame(const Pad *p) {
  int d, c = tcell, rows[8], nr = tile_rows(c, rows);
  if (nr == 1) { propSq = rf >> 1; prow = 0; propsBack = M_ROOM; mode = M_PROPS; return; }   // nothing to set: the cell's
  if (trow >= nr) trow = nr - 1;
  if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) {
    int sd = PADDIR[d];
    if (sd == 0) trow = (trow + nr - 1) % nr; else if (sd == 2) trow = (trow + 1) % nr;
    else {
      int st = sd == 1 ? 1 : -1, r = rows[trow];
      int needs = L.kind[c] == FK_PLATE || L.kind[c] == FK_SENSOR || (L.btn[c] && L.kind[c] == FK_FLOOR && !L.target[c]);
      if (r == T_SENDS) { int k = sig_cycle(L.btn[c], st, !needs); L.btn[c] = (uint8_t)(needs && !k ? next_signal() : k); }
      else if (r == T_WORKED) L.drv[c] = (uint8_t)sig_cycle(L.drv[c], st, 1);
      else if (r == T_STARTS) L.aux[c] ^= 0x80;   // a gate open / shut, a teleporter off / on
      else if (r == T_LOOKS) L.aux[c] ^= 0x40;
    }
  }
  if (pressed(p, PAD_A) && rows[trow] == T_CELL) { propSq = rf >> 1; prow = 0; propsBack = M_ROOM; mode = M_PROPS; return; }
  if (pressed(p, PAD_B) || pressed(p, PAD_X)) mode = M_ROOM;
}

// ---- wiring: the level-wide input and the logic pieces ------------------------------------------------------------
static const char *OPNAMES[] = { "AND", "OR", "XOR", "NOT", "LATCH", "COUNT", "DELAY" };
#define NOPS 7
// rows: 0 all targets; 1..T timers; T+1 add a timer; then the logic pieces; last: add a piece
static void wire_frame(const Pad *p) {
  int d, T = L.ntimer, nr = 3 + T + L.nlogic, addT = T + 1, firstL = T + 2, addL = nr - 1;
  if (wrow >= nr) wrow = nr - 1;
  if (pressed(p, PAD_L)) wfield = wfield > 0 ? wfield - 1 : 0;
  if (pressed(p, PAD_R)) wfield = wfield < 6 ? wfield + 1 : 6;
  if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) {
    int sd = PADDIR[d];
    if (sd == 0) wrow = (wrow + nr - 1) % nr; else if (sd == 2) wrow = (wrow + 1) % nr;
    else {
      int st = sd == 1 ? 1 : -1;
      if (wrow == 0) L.allSig = sig_cycle(L.allSig, st, 1);
      else if (wrow <= T) {                      // a timer: fields 0 signal, 1 kind, 2 n
        FoldTimer *q = &L.timer[wrow - 1];
        if (wfield == 0) q->sig = (uint8_t)sig_cycle(q->sig, st, 0);
        else if (wfield == 1) q->kind ^= 1;
        else { int n = q->n + st; q->n = (uint8_t)(n < 1 ? 1 : n > 255 ? 255 : n); }
      } else if (wrow >= firstL && wrow < addL) {   // a piece: 0 out, 1 op, 2 count / delay, 3..6 inputs
        FoldLogic *q = &L.logic[wrow - firstL];
        if (wfield == 0) q->out = (uint8_t)sig_cycle(q->out, st, 0);
        else if (wfield == 1) q->op = (uint8_t)((q->op + st + NOPS) % NOPS);
        else if (wfield == 2) { int mx = q->op == FL_DELAY ? 8 : 99, n = q->count + st; q->count = (uint8_t)(n < 1 ? 1 : n > mx ? mx : n); }
        else q->in[wfield - 3] = (uint8_t)sig_cycle(q->in[wfield - 3], st, 1);
      }
    }
  }
  if (pressed(p, PAD_A) && wrow == addT && L.ntimer < FOLD_MAXTIMER) {      // a new step limit, sending a fresh signal
    FoldTimer *q = &L.timer[L.ntimer]; q->kind = FT_MOVES; q->n = 20; q->sig = (uint8_t)next_signal(); L.ntimer++; wrow = L.ntimer; wfield = 2;
  }
  if (pressed(p, PAD_A) && wrow == addL && L.nlogic < FOLD_MAXLOGIC) {      // a new AND, sending a fresh signal
    FoldLogic *q = &L.logic[L.nlogic]; memset(q, 0, sizeof *q);
    q->op = FL_AND; q->count = 2; q->n = 4; q->out = (uint8_t)next_signal(); L.nlogic++; wrow = firstL + L.nlogic - 1; wfield = 3;
  }
  if (pressed(p, PAD_X)) {                                                  // delete a timer / piece
    if (wrow >= 1 && wrow <= T) { memmove(&L.timer[wrow - 1], &L.timer[wrow], sizeof(FoldTimer) * (L.ntimer - wrow)); L.ntimer--; }
    else if (wrow >= firstL && wrow < addL) { int k = wrow - firstL; memmove(&L.logic[k], &L.logic[k + 1], sizeof(FoldLogic) * (L.nlogic - k - 1)); L.nlogic--; }
  }
  if (pressed(p, PAD_B) || pressed(p, PAD_START)) mode = M_SHAPE;
}

// ---- menu and playtest -------------------------------------------------------------------------------------------
static const char *MENU[] = { "PLAYTEST", "VMX WINDOW", "WIRING", "SAVE", "LOAD", "NEW LEVEL", "QUIT" };
#define NMENU 7
static char files[64][64]; static int nfiles;
static void start_play(void) {
  build(&PL);
  if (PL.player < 0) { say("PUT THE PLAYER IN A CELL FIRST"); mode = M_SHAPE; return; }
  fold_start(&G, &PL); popT = wonT = 0; mode = M_PLAY;
}
static void menu_frame(const Pad *p) {
  int d;
  if (mrow == 4 && !nfiles) nfiles = list_levels(files, 64);
  if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) {
    int sd = PADDIR[d];
    if (sd == 0) mrow = (mrow + NMENU - 1) % NMENU; else if (sd == 2) mrow = (mrow + 1) % NMENU;
    else if (mrow == 4 && nfiles) fileIx = (fileIx + (sd == 1 ? 1 : nfiles - 1)) % nfiles;
    else if (mrow == 1) vmxWin ^= 1;
  }
  if (pressed(p, PAD_A)) {
    char full[400];
    switch (mrow) {
      case 0: start_play(); return;
      case 1: vmxWin ^= 1; break;
      case 2: wrow = 0; wfield = 0; mode = M_WIRE; return;
      case 3:
        if (!path[0]) { for (int k = 1; k < 1000; k++) { snprintf(path, sizeof path, "%s/user-%03d.fold", levelsDir, k); FILE *f = fopen(path, "r"); if (!f) break; fclose(f); } }
        say(app_save(path) ? "SAVED" : "COULD NOT SAVE"); nfiles = 0; break;
      case 4: if (nfiles) { snprintf(full, sizeof full, "%s/%s", levelsDir, files[fileIx]); say(app_load(full) ? "LOADED" : "COULD NOT LOAD"); mode = M_SHAPE; } return;
      case 5: new_level(); say("NEW LEVEL"); mode = M_SHAPE; return;
      case 6: quit = 1; return;
    }
  }
  if (pressed(p, PAD_B) || pressed(p, PAD_START)) mode = M_SHAPE;
}
static void play_frame(const Pad *p) {
  if (talkOn) {                               // reading what a tile says: A / B turn the page, then close
    if (pressed(p, PAD_A) || pressed(p, PAD_B)) { static uint8_t tmp[64 * 64]; if (++talkPage >= vmx_talk(tmp, talk, 0)) talkOn = 0; }
    if (pressed(p, PAD_START)) talkOn = 0;
    return;
  }
  if (popT) { popT--; return; }
  if (wonT) { wonT++; if (pressed(p, PAD_A) || pressed(p, PAD_B)) mode = M_SHAPE; return; }
  int d;
  if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) {
    int r = fold_move(&G, PADDIR[d]);
    popWord = r & FOLD_FLIPPED ? "FLIPPED" : r & FOLD_TURNED ? "TURNED" : r & FOLD_SHIFTED ? "SHIFTED" : r & FOLD_HINGED ? "HINGED" : NULL;
    if (popWord) popT = 30;
    if (fold_solved(&G)) wonT = 1;
  }
  if (pressed(p, PAD_A)) {                    // talk to the tile you face (or the one you stand on)
    int c = fold_facing(&G);
    if (!fold_describe(&G, c, talk, sizeof talk)) fold_describe(&G, G.p, talk, sizeof talk);
    if (talk[0]) { talkOn = 1; talkPage = 0; }
  }
  if (p->held & PAD_B) { if (++holdB == 20) { fold_start(&G, &PL); popT = 0; say("RESTART"); } }   // B undo, held: restart
  else { if (holdB > 0 && holdB < 20) fold_undo(&G); holdB = 0; }
  if (pressed(p, PAD_START)) mode = M_SHAPE;
}

// ---- frame -------------------------------------------------------------------------------------------------------
void app_init(const char *dir) { snprintf(levelsDir, sizeof levelsDir, "%s", dir); new_level(); }
int app_quit(void) { return quit; }
int app_state(char *b, int n) {                // a line of the editor's state, for comparing runs (tests)
  return snprintf(b, (size_t)n, "m%d r%d,%d,%d,%d p%d w%d,%d i%d c%d s%d l%d,%d", mode, rx, ry, rf, rrot, mrow, wrow, wfield, item, L.nsq, L.player, L.nlogic, L.logic[0].op);
}
void app_frame(const Pad *p) {
  t++; if (msgT) msgT--;
  icons_update(p);
  camera();
  switch (mode) {
    case M_SHAPE: shape_frame(p); break;
    case M_ROOM: room_frame(p); break;
    case M_PROPS: props_frame(p); break;
    case M_MENU: menu_frame(p); break;
    case M_PLAY: play_frame(p); break;
    case M_TILE: tile_frame(p); break;
    case M_WIRE: wire_frame(p); break;
  }
  prev = p->held;
}

// ---- drawing -----------------------------------------------------------------------------------------------------
static uint32_t texbuf[2][56 * 56];
static uint8_t bits[64 * 64];
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
    } else if (L.kind[c] == FK_LIGHT) {        // a light: a dark post with a lamp on top, glowing when lit
      static uint32_t postPx[1] = { 0x2c3428 }, onPx[1] = { 0xfff27a }, offPx[1] = { 0x56604f };
      Tex post = { 1, 1, postPx }, lamp = { 1, 1, (L.aux[c] & 0x80) ? onPx : offPx };
      block(vadd(p, vadd(vsc(du, 0.38f), vsc(dv, 0.38f))), vsc(du, 0.24f), vsc(dv, 0.24f), n, cs * 0.9f, &post, &post, NULL);
      block(vadd(vadd(p, vadd(vsc(du, 0.22f), vsc(dv, 0.22f))), vsc(n, cs * 0.9f)), vsc(du, 0.56f), vsc(dv, 0.56f), n, cs * 0.5f, &lamp, &lamp, NULL);
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
// the ground the shape stands on: below its lowest point
static float ground_z(void) {
  float z = (float)co[2];
  for (int i = 0; i < L.nsq; i++) { const FoldSquare *q = &L.sq[i]; for (int k = 0; k < 4; k++) { float c = q->o[2] + ((k == 1 || k == 2) ? q->u[2] : 0) + (k >= 2 ? q->v[2] : 0); if (c < z) z = c; } }
  return z - 0.02f;
}
static void draw_ground(void) {               // a soft grid fading into the distance, and the shape's shadow on it
  float zg = ground_z(); int cx = (int)floorf(look.x), cy = (int)floorf(look.y);
  for (int a = -8; a <= 8; a++) for (int b = -8; b < 8; b++) {   // cell by cell, fading with distance from the cursor
    float d = sqrtf((float)(a * a + (b + 0.5f) * (b + 0.5f))) / 8.0f; if (d >= 1) continue;
    float k = (1 - d) * (1 - d); int major = (cx + a) % 4 == 0;
    uint32_t c = ((int)(0x10 + (major ? 0x16 : 0x0a) * k) << 16) | ((int)(0x18 + (major ? 0x2a : 0x16) * k) << 8) | (int)(0x10 + (major ? 0x12 : 0x08) * k);
    gfx_line3(v3f((float)(cx + a), (float)(cy + b), zg), v3f((float)(cx + a), (float)(cy + b + 1), zg), c, 1);
    major = (cy + a) % 4 == 0;
    c = ((int)(0x10 + (major ? 0x16 : 0x0a) * k) << 16) | ((int)(0x18 + (major ? 0x2a : 0x16) * k) << 8) | (int)(0x10 + (major ? 0x12 : 0x08) * k);
    gfx_line3(v3f((float)(cx + b), (float)(cy + a), zg), v3f((float)(cx + b + 1), (float)(cy + a), zg), c, 1);
  }
  const float lx = 0.38f, ly = 0.52f, lz = 0.76f;   // the same light as the shading
  gfx_shadow_begin();
  for (int i = 0; i < L.nsq; i++) {
    const FoldSquare *q = &L.sq[i]; Vec3 P[4];
    for (int k = 0; k < 4; k++) {
      float x = q->o[0] + ((k == 1 || k == 2) ? q->u[0] : 0) + (k >= 2 ? q->v[0] : 0), y = q->o[1] + ((k == 1 || k == 2) ? q->u[1] : 0) + (k >= 2 ? q->v[1] : 0);
      float z = q->o[2] + ((k == 1 || k == 2) ? q->u[2] : 0) + (k >= 2 ? q->v[2] : 0), t = (z - zg) / lz;
      P[k] = v3f(x - lx * t, y - ly * t, zg);
    }
    gfx_shadow_quad(P);
  }
  gfx_shadow_end(0.55f);
}
static void draw_shape(void) {
  gfx_zclear(); tex_init(); gfx_camera(VPm);   // (the icons set their own cameras after this)
  gfx_fog(0x070b06, dist * 0.9f, dist * 3.2f);
  draw_ground();
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
  // the working plane near the cursor (a small patch, so it never streaks off at a low angle), and the cursor
  Vec3 o = v3f(co[0], co[1], co[2]), a = v3f(ce1[0], ce1[1], ce1[2]), b = v3f(ce2[0], ce2[1], ce2[2]);
  const int R = 2;
  for (int i = -R; i <= R + 1; i++) for (int j = -R; j <= R; j++) {   // edge by edge, fading out from the cursor
    float d = sqrtf((i - 0.5f) * (i - 0.5f) + j * j) / (R + 1.2f); if (d >= 1) continue;
    float k = 1 - d; uint32_t c = ((int)(0x0e + 0x22 * k) << 16) | ((int)(0x16 + 0x48 * k) << 8) | (int)(0x0e + 0x1c * k);
    Vec3 p0 = vadd(o, vadd(vsc(a, (float)i), vsc(b, (float)j))), q0 = vadd(o, vadd(vsc(b, (float)i), vsc(a, (float)j)));
    gfx_line3(p0, vadd(p0, b), c, 1); gfx_line3(q0, vadd(q0, a), c, 1);
  }
  Vec3 C[4] = { o, vadd(o, a), vadd(vadd(o, a), b), vadd(o, b) };
  int here = find_sq(co, ce1, ce2);
  float pulse = 0.5f + 0.5f * sinf(t * 0.2f);
  if (here < 0) gfx_quad_alpha(C, 0x7dff3a, 0.12f + 0.1f * pulse);        // where a cell would go: a faint ghost
  uint32_t cc = here < 0 ? 0xd8ff9c : 0xffd060;                             // amber over a cell that is there
  for (int k = 0; k < 4; k++) {                                             // glowing corner brackets
    Vec3 c0 = C[k], toN = vadd(C[(k + 1) & 3], vsc(c0, -1)), toP = vadd(C[(k + 3) & 3], vsc(c0, -1));
    float x0, y0, x1, y1, x2, y2;
    if (!gfx_project(c0, &x0, &y0) || !gfx_project(vadd(c0, vsc(toN, 0.3f)), &x1, &y1) || !gfx_project(vadd(c0, vsc(toP, 0.3f)), &x2, &y2)) continue;
    for (int w = -1; w <= 1; w++) for (int h = -1; h <= 1; h++) {
      gfx_line((int)x0 + w, (int)y0 + h, (int)x1 + w, (int)y1 + h, w || h ? 0x3f7f2c : cc);
      gfx_line((int)x0 + w, (int)y0 + h, (int)x2 + w, (int)y2 + h, w || h ? 0x3f7f2c : cc);
    }
  }
  for (int k = 0; k < 4; k++) gfx_line3(C[k], C[(k + 1) & 3], pulse > 0.5f ? cc : 0x6fb050, 1);
}
// the VMX window: the owner's render of the VMX (assets/vmx_small / vmx_large) with the 64x64 screen in it
typedef struct { int w, h, sx, sy; uint8_t *px; } Frame;
static Frame frames[2]; static int framesTried;
static uint8_t *metalPx;
static void load_frames(void) {
  framesTried = 1;
  FILE *mf = fopen("assets/metal.rgb", "rb");   // the brushed metal for panels
  if (mf) {
    unsigned char hd[8];
    if (fread(hd, 1, 8, mf) == 8 && !memcmp(hd, "METL", 4)) {
      int w = hd[4] | hd[5] << 8, h = hd[6] | hd[7] << 8;
      metalPx = malloc((size_t)w * h * 3);
      if (metalPx && fread(metalPx, 3, (size_t)w * h, mf) == (size_t)w * h) gfx_metal_texture(metalPx, w, h);
    }
    fclose(mf);
  }
  const char *names[2] = { "assets/vmx_small.rgba", "assets/vmx_large.rgba" };
  for (int k = 0; k < 2; k++) {
    FILE *f = fopen(names[k], "rb"); if (!f) continue;
    unsigned char hd[12];
    if (fread(hd, 1, 12, f) == 12 && !memcmp(hd, "VMXF", 4)) {
      Frame *F = &frames[k]; F->w = hd[4] | hd[5] << 8; F->h = hd[6] | hd[7] << 8; F->sx = hd[8] | hd[9] << 8; F->sy = hd[10] | hd[11] << 8;
      F->px = malloc((size_t)F->w * F->h * 4);
      if (F->px && fread(F->px, 4, (size_t)F->w * F->h, f) != (size_t)F->w * F->h) { free(F->px); F->px = NULL; }
    }
    fclose(f);
  }
}
// draws the VMX with its screen's top-left at (x, y); scale 2 uses the small frame, 3 the large; returns its width
static int draw_vmx(int x, int y, int scale, const uint8_t *b) {
  if (!framesTried) load_frames();
  Frame *F = &frames[scale >= 3 ? 1 : 0];
  static uint32_t col[64 * 64]; vmx_colours(b, col, VMX_LIT, VMX_DARK);
  Tex tx = { 64, 64, col };
  if (F->px && scale == (scale >= 3 ? 3 : 2)) {
    int ox = x - F->sx, oy = y - F->sy;
    for (int j = 0; j < F->h; j++) for (int i = 0; i < F->w; i++) {
      int X = ox + i, Y = oy + j; if (X < 0 || Y < 0 || X >= GW || Y >= GH) continue;
      const uint8_t *q = F->px + 4 * (j * F->w + i); unsigned a = q[3]; if (!a) continue;
      uint32_t d = gfx_px[Y * GW + X];
      unsigned r = (q[0] * a + ((d >> 16) & 255) * (255 - a)) / 255, g = (q[1] * a + ((d >> 8) & 255) * (255 - a)) / 255, bl = (q[2] * a + (d & 255) * (255 - a)) / 255;
      gfx_px[Y * GW + X] = r << 16 | g << 8 | bl;
    }
    gfx_blit(x, y, scale, &tx);
    return F->w;
  }
  int w = 64 * scale;                          // no render: a plain body
  gfx_rect(x - 14, y - 14, w + 28, w + 46, 0x2a2d2a); gfx_frame(x - 14, y - 14, w + 28, w + 46, 0x4a4f48);
  gfx_rect(x - 3, y - 3, w + 6, w + 6, 0x000000);
  gfx_blit(x, y, scale, &tx);
  gfx_text(x + w / 2 - gfx_text_w(2, "VMX") / 2, y + w + 12, 2, 0x8c9787, "VMX");
  return w + 28;
}
static uint32_t item_colour(int k) {          // a palette icon's colour (the tile's colour in 3D)
  switch (k) { case 0: case 16: return 0x7dff3a; case 2: return 0xe0c070; case 5: return 0xf2f6ff; case 6: return 0xbfefff; case 7: return 0xffd040;
               case 8: return 0x8a9a86; case 9: return 0xe070ff; case 10: return 0xff9a3c; case 11: return 0x9fb4c8; case 12: case 13: return 0xffc83c;
               case 14: return 0xa0a8a0; case 17: return 0xfff27a; }
  return 0x7dff3a;
}
static void draw_room_editor(void) {
  memset(bits, 0, sizeof bits);
  vmx_room(bits, L.wall, L.target, L.box, L.btn, L.kind, L.aux, rf, rrot, L.player, &S, 0);
  const int S6 = 6, X0 = 30, Y0 = 62;          // the room 6x bigger: a cell is 42 px
  char s[64];
  snprintf(s, sizeof s, "CELL %d  %s SIDE", rf >> 1, rf & 1 ? "BACK" : "FRONT");
  gfx_panel(X0 - 12, Y0 - 40, 56 * S6 + 24, 56 * S6 + 52, s, C_HI);
  for (int y = 0; y < 56; y++) for (int x = 0; x < 56; x++) gfx_rect(X0 + x * S6, Y0 + y * S6, S6, S6, bits[(y + 8) * 64 + x + 4] ? VMX_LIT : VMX_DARK);
  uint32_t cc = (t >> 3) & 1 ? C_CURSOR : 0xffffff;
  for (int w = 0; w < 3; w++) gfx_frame(X0 + rx * 42 - w, Y0 + ry * 42 - w, 42 + 2 * w, 42 + 2 * w, cc);
  // the tile palette: icons in a grid, the picked one lit up and named
  int px0 = 410, py0 = 22, cols = 4;
  gfx_panel(px0, py0, 216, 300, "TILES", C_HI);
  for (int k = 0; k < NITEMS; k++) {
    int bx = px0 + 14 + (k % cols) * 48, by = py0 + 44 + (k / cols) * 46;
    if (k == item) { gfx_rect(bx - 4, by - 4, 44, 44, 0x2f5a24); gfx_frame(bx - 4, by - 4, 44, 44, C_HI); }
    const char **spr = vmx_icon(ITEMS[k]); uint32_t ic = item_colour(k);
    gfx_rect(bx, by, 36, 36, 0x0a120a);
    for (int j = 0; j < 7; j++) for (int i2 = 0; i2 < 7; i2++) if (spr[j][i2] == '#') gfx_rect(bx + 1 + i2 * 5, by + 1 + j * 5, 5, 5, ic);
  }
  gfx_text5(px0 + 14, py0 + 270, 2, C_HI, ITEMS[item]);
  gfx_hint(px0 + 4, py0 + 312, "R-STICK", "PICK A TILE");
}
// ---- the level view's layout (the owner's sketch, 2026-10-20): a row of tabs along the top, the green scene, and the
// controls as pictures on brushed-metal panels in the bottom corners
static const char *TABS[] = { "LEVEL", "CELL", "WIRING", "TEST", "SAVE", "LOAD" };
static void tab_row(int current) {            // along the top of the screen
  const int y0 = 4, y1 = 34, n = 6, W = GW / n;
  gfx_dim(0, 0, GW, y1 + 2, 0.45f);
  uint32_t line = 0xc8c8c8;
  gfx_thick(0, y0, GW, y0, 2, line);
  for (int k = 0; k < n; k++) {
    int x0 = k * W, x1 = x0 + W;
    if (k == current) for (int y = y0 + 2; y < y1; y++) { int s = (y - y0) / 3; gfx_rect(x0 + 4 + s, y, W - 10, 1, 0x1f3a18); }
    if (k) gfx_thick(x0 + 2, y0 + 4, x0 - 8, y1, 2, line);                // the slanted dividers
    gfx_thick(x0 + (k ? 0 : 4), y1, x1 - 10, y1, 2, line);
    gfx_text5(x0 + W / 2 - gfx_text5_w(1, TABS[k]) / 2 - 2, y0 + 12, 1, k == current ? 0xe8ffd8 : 0x9aa294, TABS[k]);
  }
}
static void level_controls(void) {   // the metal panels in the corners, each with its live 3D control (icons3d.c)
  gfx_metal_panel(0, 296, 150, 92, 14, 2 | 4); icons_stick(4, 298, 142, 88);     // the stick rolls the working plane
  gfx_metal_panel(0, 398, 138, 82, 14, 2); icons_dpad(4, 400, 130, 80);           // the D-pad moves the cursor
  gfx_metal_panel(GW - 128, 250, 128, 142, 14, 1 | 8); icons_buttons(GW - 124, 254, 120, 134);   // the face buttons
  gfx_metal_panel(GW - 168, 400, 168, 80, 14, 1); icons_camera(GW - 164, 402, 160, 78);         // the camera
}
static void top_bar(const char *mode) {
  gfx_dim(0, 0, GW, 30, 0.55f); gfx_rect(0, 30, GW, 1, 0x2f5a24);
  gfx_text5(12, 8, 2, C_HI, "FOLD");
  gfx_text5(70, 10, 1, C_DIM, L.name);
  int w = gfx_text5_w(1, mode) + 16; gfx_rect(GW - w - 10, 7, w, 16, 0x2f5a24); gfx_text5(GW - w - 2, 12, 1, 0xe8ffd8, mode);
}
static void hints(const char **pairs, int n) {   // the bottom bar: badges and what they do
  gfx_dim(0, GH - 30, GW, 30, 0.6f); gfx_rect(0, GH - 31, GW, 1, 0x2f5a24);
  int x = 10;
  for (int k = 0; k < n; k++) x += gfx_hint(x, GH - 24, pairs[2 * k], pairs[2 * k + 1]);
}
static void row(int x, int y, int w, int sel, const char *s) {
  if (sel) { gfx_rect(x - 6, y - 5, w, 20, 0x23421b); gfx_rect(x - 6, y - 5, 3, 20, C_HI); }
  gfx_text5(x, y, 1, sel ? 0xe8ffd8 : C_TEXT, s);
}
void app_draw(void) {
  char s[160];
  gfx_gradient(0x142012, 0x040604);
  if (mode == M_PLAY) {
    static uint8_t b[64 * 64];
    vmx_game(b, &G, 1, popT ? popWord : NULL);
    if (talkOn) vmx_talk(b, talk, talkPage);
    if (wonT) { for (int y = 24; y < 40; y++) for (int x = 6; x < 58; x++) b[y * 64 + x] = 0; vmx_text(b, 20, 26, "CLEAR!"); snprintf(s, sizeof s, "%d MOVES", G.moves); vmx_text(b, 32 - 2 * (int)strlen(s) + 1, 33, s); }
    draw_vmx(GW / 2 - 96, 130, 3, b);
    top_bar("PLAYTEST");
    const char *h[] = { "DPAD", "MOVE", "A", "TALK", "B", "UNDO", "HOLD B", "RESTART", "START", "BACK" };
    hints(h, 5);
    return;
  }
  int roomish = mode == M_ROOM || mode == M_TILE || (mode == M_PROPS && propsBack == M_ROOM);
  if (roomish) {
    draw_room_editor();
    top_bar("CELL EDITOR");
    const char *h[] = { "DPAD", "MOVE", "A", "PLACE", "B", "FLOOR", "STICK", "NEXT CELL", "L R", "TURN", "X", "SETTINGS", "Y", "LEVEL" };
    if (mode == M_ROOM) hints(h, 7);
  } else {
    if (!framesTried) load_frames();
    gfx_view_offset(0, 16);                   // (the green gradient behind it is drawn above)
    draw_shape();
    gfx_view_offset(0, 0);
    tab_row(mode == M_WIRE ? 2 : mode == M_MENU ? 3 : 0);
    snprintf(s, sizeof s, "%s   %d CELLS", L.name, L.nsq); gfx_text5(GW - 12 - gfx_text5_w(1, s), 44, 1, 0x7f8a78, s);
    if (mode == M_SHAPE) level_controls();
    if (vmxWin) {                             // the VMX window: the cell under the cursor, as on the handheld
      static uint8_t b[64 * 64]; memset(b, 0, sizeof b);
      int i = find_sq(co, ce1, ce2);
      if (i >= 0) { int f = sq_face_toward_eye(i); if (sides[i] >> (f & 1) & 1) vmx_room(b, L.wall, L.target, L.box, L.btn, L.kind, L.aux, f, 0, L.player, &S, 0); else vmx_text(b, 12, 28, "NO ROOM"); }
      else vmx_text(b, 4, 28, "EMPTY SPACE");
      draw_vmx(GW / 2 - 96, GH / 2 - 96 + 20, 3, b);   // in the middle of the screen
    }
  }
  if (mode == M_PROPS) {
    int i = propSq, rows[8], nr = prop_rows(i, rows), h = 50 + nr * 26 + 30;
    snprintf(s, sizeof s, "CELL %d SETTINGS", i);
    gfx_panel(120, 110, 400, h, s, C_HI);
    for (int k = 0; k < nr; k++) {
      char r[64];
      switch (rows[k]) {
        case P_MOVE: snprintf(r, sizeof r, "MOVE: %s", FUNCS[func_index(&func[i])]); break;
        case P_SIGNAL: if (trig[i]) snprintf(r, sizeof r, "WORKED BY SIGNAL: %d", trig[i]); else snprintf(r, sizeof r, "WORKED BY SIGNAL: NONE"); break;
        case P_TURNMODE: snprintf(r, sizeof r, "TURNS: %s", func[i].kind == 'R' ? "WHILE ON, BACK WHEN OFF" : "A QUARTER EACH CHANGE"); break;
        default: snprintf(r, sizeof r, "DELETE CELL  (A)");
      }
      row(140, 156 + k * 26, 370, k == prow, r);
    }
    const char *hh[] = { "DPAD", "PICK / CHANGE", "B", "DONE" }; hints(hh, 2);
  }
  if (mode == M_TILE) {
    int c = tcell, rows[8], nr = tile_rows(c, rows);
    static FoldGame tg; build(&PL); fold_start(&tg, &PL); char said[FOLD_TEXTLEN * 2];
    if (!fold_describe(&tg, c, said, sizeof said)) snprintf(said, sizeof said, "(NOTHING)");
    int h = 50 + nr * 26 + 70;
    gfx_panel(70, 100, 500, h, "TILE SETTINGS", C_HI);
    for (int k = 0; k < nr; k++) {
      char r[64];
      switch (rows[k]) {
        case T_SENDS: if (L.btn[c]) snprintf(r, sizeof r, "SENDS SIGNAL: %d", L.btn[c]); else snprintf(r, sizeof r, "SENDS SIGNAL: NONE"); break;
        case T_WORKED: if (L.drv[c]) snprintf(r, sizeof r, "WORKED BY SIGNAL: %d", L.drv[c]); else snprintf(r, sizeof r, "WORKED BY SIGNAL: NONE"); break;
        case T_STARTS: snprintf(r, sizeof r, "STARTS: %s", L.kind[c] == FK_TELE ? (L.aux[c] & 0x80 ? "OFF" : "ON") : L.kind[c] == FK_LIGHT ? (L.aux[c] & 0x80 ? "LIT" : "DARK") : (L.aux[c] & 0x80 ? "OPEN" : "SHUT")); break;
        case T_LOOKS: snprintf(r, sizeof r, "LOOKS LIKE: %s", L.aux[c] & 0x40 ? "A BRICK WALL" : "A GATE"); break;
        default: snprintf(r, sizeof r, "CELL SETTINGS  (A)");
      }
      row(92, 146 + k * 26, 470, k == trow, r);
    }
    int ty = 146 + nr * 26 + 10;
    gfx_text5(90, ty, 1, C_DIM, "IT SAYS:");
    char line[80]; int at = 0, li = 0, n = (int)strlen(said);
    while (at < n && li < 3) { int take = n - at > 66 ? 66 : n - at; if (take == 66) while (take > 1 && said[at + take] != ' ') take--; memcpy(line, said + at, (size_t)take); line[take] = 0; gfx_text5(150, ty + li * 14, 1, 0xe8ffd8, line); at += take; while (said[at] == ' ') at++; li++; }
    const char *hh[] = { "DPAD", "PICK / CHANGE", "B", "DONE" }; hints(hh, 2);
  }
  if (mode == M_WIRE) {
    int T = L.ntimer, addT = T + 1, firstL = T + 2, addL = 3 + T + L.nlogic - 1, y = 106;
    gfx_panel(30, 52, 580, 390, "WIRING", C_HI);
    char r[96];
    if (L.allSig) snprintf(r, sizeof r, "ALL TARGETS FILLED  SENDS %d", L.allSig); else snprintf(r, sizeof r, "ALL TARGETS FILLED  SENDS NONE");
    row(56, y, 540, wrow == 0, r); y += 30;
    gfx_text5(50, y - 6, 1, C_DIM, "TIMERS"); y += 12;
    for (int k = 0; k < T; k++, y += 22) {
      const FoldTimer *q = &L.timer[k]; int sel = wrow == k + 1, x = 70; char fld[3][24];
      snprintf(fld[0], 24, "%d =", q->sig); snprintf(fld[1], 24, "%s", q->kind == FT_TICK ? "EVERY" : "AFTER"); snprintf(fld[2], 24, "%d MOVES", q->n);
      row(56, y, 540, sel, "");
      for (int f2 = 0; f2 < 3; f2++) { gfx_text5(x, y, 1, sel && f2 == wfield ? 0xffe070 : sel ? 0xe8ffd8 : C_TEXT, fld[f2]); x += gfx_text5_w(1, fld[f2]) + 14; }
    }
    row(56, y, 540, wrow == addT, "+ ADD A TIMER / STEP LIMIT"); y += 30;
    gfx_text5(50, y - 6, 1, C_DIM, "LOGIC"); y += 12;
    for (int k = 0; k < L.nlogic && y < 400; k++, y += 22) {
      const FoldLogic *q = &L.logic[k]; int x = 70, sel = wrow == firstL + k;
      char fld[7][16];
      snprintf(fld[0], 16, "%d =", q->out); snprintf(fld[1], 16, "%s", OPNAMES[q->op]);
      if (q->op == FL_COUNT) snprintf(fld[2], 16, "%d OF", q->count); else if (q->op == FL_DELAY) snprintf(fld[2], 16, "%d BY", q->count); else snprintf(fld[2], 16, " ");
      for (int i2 = 0; i2 < 4; i2++) { if (q->in[i2]) snprintf(fld[3 + i2], 16, "%d", q->in[i2]); else snprintf(fld[3 + i2], 16, "-"); }
      row(56, y, 540, sel, "");
      for (int f2 = 0; f2 < 7; f2++) { gfx_text5(x, y, 1, sel && f2 == wfield ? 0xffe070 : sel ? 0xe8ffd8 : C_TEXT, fld[f2]); x += gfx_text5_w(1, fld[f2]) + 14; }
    }
    row(56, y, 540, wrow == addL, "+ ADD A LOGIC PIECE");
    const char *hh[] = { "DPAD", "PICK / CHANGE", "L R", "FIELD", "A", "ADD", "X", "DELETE", "B", "DONE" }; hints(hh, 5);
  }
  if (mode == M_MENU) {
    gfx_panel(190, 90, 260, 46 + NMENU * 30, "MENU", C_HI);
    for (int k = 0; k < NMENU; k++) {
      char rr[96];
      if (k == 1) snprintf(rr, sizeof rr, "VMX WINDOW: %s", vmxWin ? "ON" : "OFF");
      else if (k == 4) snprintf(rr, sizeof rr, "LOAD: %s", nfiles ? files[fileIx] : "-");
      else snprintf(rr, sizeof rr, "%s", MENU[k]);
      row(212, 136 + k * 30, 224, k == mrow, rr);
    }
    const char *hh[] = { "DPAD", "PICK", "A", "CHOOSE", "B", "BACK" }; hints(hh, 3);
  }
  if (msgT) { int w = gfx_text5_w(2, msg); gfx_dim(GW / 2 - w / 2 - 14, GH - 74, w + 28, 30, 0.7f); gfx_rect(GW / 2 - w / 2 - 14, GH - 74, 3, 30, C_HI); gfx_text5(GW / 2 - w / 2, GH - 66, 2, 0xe8ffd8, msg); }
}
