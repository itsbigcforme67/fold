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
enum { M_SHAPE, M_ROOM, M_PROPS, M_MENU, M_PLAY, M_TILE, M_WIRE, M_KEYS };

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
// eases towards the frame (vn / vr / vu); the right stick adds a look round (orbit, pitch), L / R zoom.
static int Fn[3] = { 0, 0, 1 }, Fr[3] = { 1, 0, 0 }, Fu[3] = { 0, 1, 0 };
static Vec3 vn = { 0, 0, 1 }, vr = { 1, 0, 0 }, vu = { 0, 1, 0 };
static float orbit = 0, pitch = 0.9f, dist = 5.5f;
static int flipT, flipN[3], flipR[3];   // swinging round to the other side: the second quarter turn, after flipT frames
static Vec3 look = { 0.5f, 0.5f, 0 }, eye;
static Mat4 VPm;
// room editor
static int rf, rrot, rx = 3, ry = 3, item = 1, inPal, palFrom;   // inPal: the cursor is on the tile palette
static const char *ITEMS[] = { "WALL", "FLOOR", "BOX", "TARGET", "BUTTON", "PLAYER", "ICE", "ONE-WAY", "PIT", "TELEPORTER", "PLATE", "GATE", "KEY", "LOCK", "GRATE", "SENSOR", "WALL GATE", "LIGHT", "SIGN", "KEYPAD" };
#define NITEMS 20
// settings / menu
static int propFace, propRot;   // the side the cell settings' layout tools work on, and its turn on screen
static int prow, mrow, fileIx, vmxWin = 0, propSq, propsBack = M_SHAPE, trow, tcell;
// playtest
static FoldLevel PL; static FoldGame G; static int popT, wonT, holdB, bannerT, bannerSq = -1; static const char *popWord;
static int kpOn, kpCell, kpSel, kpMsgT; static char kpBuf[8]; static const char *kpMsg;   // the keypad in play
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
  L.sq[i] = q; sides[i] = 0; func[i].kind = 0; trig[i] = 0; L.sqname[i][0] = 0;
  make_room(2 * i); clear_face(2 * i + 1);
  resurface();
  return i;
}
static void remove_sq(int i) {
  int n = L.nsq;
  uint8_t *arr[8] = { L.wall, L.target, L.box, L.btn, L.kind, L.aux, L.drv, L.txt };   // every layer moves down
  for (int k = 0; k < 8; k++) memmove(arr[k] + 2 * i * NN, arr[k] + 2 * (i + 1) * NN, (size_t)(2 * (n - i - 1) * NN));
  memmove(&L.sq[i], &L.sq[i + 1], sizeof(FoldSquare) * (n - i - 1));
  memmove(L.sqname[i], L.sqname[i + 1], sizeof L.sqname[0] * (n - i - 1));
  memmove(&sides[i], &sides[i + 1], (size_t)(n - i - 1)); memmove(&func[i], &func[i + 1], sizeof(FoldAct) * (n - i - 1));
  memmove(&trig[i], &trig[i + 1], sizeof(int) * (n - i - 1));
  for (int k = 0; k < n - 1; k++) if (func[k].kind && func[k].sq > i) func[k].sq--;
  if (L.player >= 0) { int f = L.player / NN; if (f >> 1 == i) L.player = -1; else if (f >> 1 > i) L.player -= 2 * NN; }
  L.nsq--;
  for (int k = 0; k < 8; k++) memset(arr[k] + 2 * L.nsq * NN, k == 0, 2 * NN);
  L.sqname[L.nsq][0] = 0;
  resurface();
}
// ---- signals -----------------------------------------------------------------------------------------------------
static int sig_used(int k) {                  // does anything send signal k?
  if (L.allSig == k) return 1;
  for (int i = 0; i < L.ntimer; i++) if (L.timer[i].sig == k) return 1;
  for (int i = 0; i < L.nlogic; i++) if (L.logic[i].out == k || (L.logic[i].op == FL_SHIFT && k > L.logic[i].out && k < L.logic[i].out + L.logic[i].count)) return 1;
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
  static FoldLevel B; build(&B);   // static: 77 KB, more than the Xbox main thread's stack
  FILE *f = fopen(p, "w"); if (!f) return 0;
  fprintf(f, "fold 1\nname %s\n", L.name);
  for (int i = 0; i < L.nsq; i++) { const FoldSquare *q = &L.sq[i];
    fprintf(f, "square %d %d %d  %d %d %d  %d %d %d\n", q->o[0], q->o[1], q->o[2], q->u[0], q->u[1], q->u[2], q->v[0], q->v[1], q->v[2]); }
  for (int i = 0; i < L.nsq; i++) if (L.sqname[i][0]) fprintf(f, "cellname %d %s\n", i, L.sqname[i]);
  for (int fc = 0; fc < 2 * L.nsq; fc++) {
    if (!(sides[fc >> 1] >> (fc & 1) & 1)) continue;
    fprintf(f, "room %d %c\n", fc >> 1, fc & 1 ? 'b' : 'f');
    for (int y = 0; y < N; y++) { for (int x = 0; x < N; x++) { int c = fc * NN + y * N + x;
      char ch = L.kind[c] == FK_ICE ? '~' : L.kind[c] == FK_ONEWAY ? "^>v<"[L.aux[c] & 3] : L.kind[c] == FK_PIT ? '_' : L.kind[c] == FK_TELE ? (char)('0' + L.aux[c]) : L.kind[c] == FK_PLATE ? '=' : L.kind[c] == FK_GATE ? (L.aux[c] & 0x40 ? (L.aux[c] & 0x80 ? ';' : '%') : L.aux[c] & 0x80 ? ':' : '|') : L.kind[c] == FK_SENSOR ? '*' : L.kind[c] == FK_LIGHT ? (L.aux[c] & 0x80 ? '$' : '@') : L.kind[c] == FK_KEY ? 'k' : L.kind[c] == FK_LOCK ? 'l' : L.kind[c] == FK_GRATE ? '+' : L.kind[c] == FK_SIGN ? '&' : L.kind[c] == FK_KEYPAD ? '?' :   // special floors: a box /
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
      if (L.txt[c] && L.text[L.txt[c] - 1][0]) fprintf(f, "text %d %c %d %d %s\n", fc >> 1, sd, c % N, (c / N) % N, L.text[L.txt[c] - 1]);
      if (L.kind[c] == FK_KEYPAD && L.aux[c] && L.aux[c] <= L.nkp) {
        const FoldKeypad *k = &L.kp[L.aux[c] - 1]; char nm[16]; int q = 0;
        for (const char *a = FOLD_VALUE_NAMES[k->src]; *a && q < 15; a++) nm[q++] = (char)(*a | 32); nm[q] = 0;
        fprintf(f, "keypad %d %c %d %d %s", fc >> 1, sd, c % N, (c / N) % N, nm);
        if (k->src == FV_NUMBER || k->src == FV_COUNT) fprintf(f, " %d", k->arg);
        if (k->op) fprintf(f, " %c %d", "+-*"[k->op - 1], k->n);
        fprintf(f, " %s\n", k->cmp == 1 ? ">=" : k->cmp == 2 ? "<=" : "=");
      }
      if (L.kind[c] == FK_SIGN && L.aux[c] && L.text[L.aux[c] - 1][0]) fprintf(f, "text2 %d %c %d %d %s\n", fc >> 1, sd, c % N, (c / N) % N, L.text[L.aux[c] - 1]);
    }
  }
  if (B.allSig) fprintf(f, "input %d alltargets\n", B.allSig);
  for (int k = 0; k < B.ntimer; k++) if (B.timer[k].sig) fprintf(f, "input %d %s %d\n", B.timer[k].sig, B.timer[k].kind == FT_TICK ? "tick" : "moves", B.timer[k].n ? B.timer[k].n : 1);
  static const char *OPS[] = { "and", "or", "xor", "not", "latch", "count", "delay", "shift" };
  for (int k = 0; k < B.nlogic; k++) {
    const FoldLogic *q = &B.logic[k];
    fprintf(f, "logic %d %s", q->out, OPS[q->op]);
    if (q->op == FL_COUNT || q->op == FL_DELAY || q->op == FL_SHIFT) fprintf(f, " %d", q->count ? q->count : 1);
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
static void plane_axes(Vec3 *r, Vec3 *u) {       // screen right / up as they lie on the plane: past the top or
  view_axes(r, u);                                // underneath, screen up runs the other way along it
  if (sinf(pitch) < 0) *u = v3f(-u->x, -u->y, -u->z);
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
  Vec3 r, u; plane_axes(&r, &u);
  Vec3 want = sd == 0 ? u : sd == 1 ? r : sd == 2 ? v3f(-u.x, -u.y, -u.z) : v3f(-r.x, -r.y, -r.z);
  const int *cand[2] = { Fr, Fu }; float best = -1e9f;
  for (int k = 0; k < 4; k++) {
    int sg = k & 1 ? -1 : 1; const int *a = cand[k >> 1];
    float d = sg * (a[0] * want.x + a[1] * want.y + a[2] * want.z);
    if (d > best) { best = d; for (int i = 0; i < 3; i++) out[i] = sg * a[i]; }
  }
}
static void facing_normal(int nc[3]) {   // the cursor plane's normal on the side you are looking from
  Vec3 c = cur_centre(); float d = (eye.x - c.x) * Fn[0] + (eye.y - c.y) * Fn[1] + (eye.z - c.z) * Fn[2];
  for (int i = 0; i < 3; i++) nc[i] = d < 0 ? -Fn[i] : Fn[i];
}
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
// the turn of side f's room in the cell editor that matches how it lies on screen in the level view: the editor
// draws the room's +x to the right and +y down at turn 0, so take the turn whose screen directions fit best
static int open_rot(int f) {
  const FoldSquare *q = &L.sq[f >> 1]; int sg = f & 1 ? -1 : 1;
  Vec3 ux = v3f(sg * q->u[0], sg * q->u[1], sg * q->u[2]), uy = v3f(q->v[0], q->v[1], q->v[2]), r, u;
  plane_axes(&r, &u);
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
  if (p->held & PAD_L) { dist /= 1.04f; if (dist < 1.2f) dist = 1.2f; }   // L / R: zoom in / out (held)
  if (p->held & PAD_R) { dist *= 1.04f; if (dist > 20) dist = 20; }
  if (pressed(p, PAD_LS) && !flipT) flip_side();
  if (p->held & PAD_BACK) { dist *= 1 + p->ry * 0.04f; if (dist < 1.2f) dist = 1.2f; if (dist > 20) dist = 20; }   // BACK + right stick: zoom
  else { orbit += p->rx * 0.06f; pitch += p->ry * 0.05f; }   // right stick: look round (up / down inverted, owner)
  if (pressed(p, PAD_WHITE) || pressed(p, PAD_BLACK)) {   // White / Black: raise the cursor a layer towards you / drop it
    int nc[3]; facing_normal(nc); int sg = pressed(p, PAD_WHITE) ? 1 : -1;
    for (int k = 0; k < 3; k++) co[k] += sg * nc[k];
  }
  if (pitch > 2 * PI) pitch -= 2 * PI; if (pitch < 0) pitch += 2 * PI;   // free: over the top and underneath
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
  if (pressed(p, PAD_X) && i >= 0) { propSq = i; propFace = f; propRot = open_rot(f); prow = 0; propsBack = M_SHAPE; mode = M_PROPS; }   // X: the cell's settings
  if (pressed(p, PAD_START)) { mrow = 0; mode = M_MENU; }
  if (pressed(p, PAD_RS)) vmxWin ^= 1;        // right stick click: the VMX pops up in the middle / goes away
}

// ---- room editor -------------------------------------------------------------------------------------------------
typedef struct { Vec3 o, u, v, n; } FFrame;   // a side's frame (front, or the back's mirror)
static FFrame face_frame(int f);
static void screen_axes(Vec3 *rw, Vec3 *dw);
static Vec3 vsc(Vec3 a, float k);
// the cell editor's room animations (as on the VMX): moving into the next cell slides flat or turns over the edge (an
// outside corner tips away like a cube, an inside corner folds up towards you); a right-stick turn spins the room; up /
// down turns the cell over to its other side. The old picture is kept as a texture; both are drawn on 3D quads.
enum { AN_NONE, AN_MOVE, AN_TURN, AN_FLIP };
static int anKind, anT, anLen, anSd, anRel, anDir;   // anRel: 0 flat, 1 outside corner, 2 inside corner
static uint32_t anOld[56 * 56], anNew[56 * 56];
static void room_picture(int f, int rot, uint32_t *out) {
  static uint8_t b[64 * 64]; memset(b, 0, sizeof b);
  vmx_room(b, L.wall, L.target, L.box, L.btn, L.kind, L.aux, f, rot, L.player, &S, 0);
  for (int y = 0; y < 56; y++) for (int x = 0; x < 56; x++) out[y * 56 + x] = b[(y + 8) * 64 + x + 4] ? VMX_LIT : VMX_DARK;
}
static void anim_start(int kind, int len) { anKind = kind; anT = 0; anLen = len; room_picture(rf, rrot, anOld); }
static int screen_to_cell(int sx, int sy) {   // inverse of fold_to_screen for the room on screen
  for (int c = rf * NN; c < (rf + 1) * NN; c++) { int x, y; fold_to_screen(c % N, (c / N) % N, rrot, &x, &y); if (x == sx && y == sy) return c; }
  return rf * NN;
}
static int go_room(int c, int sd) {           // step from cell c in screen direction sd into the next room
  int d = (sd + rrot) & 3, d1, c1 = fold_step(&S, c, d, &d1);
  if (c1 < 0) return 0;
  int f1 = c1 / NN;
  if (!(sides[f1 >> 1] >> (f1 & 1) & 1)) return 0;
  { // the animation: flat, or over an outside / inside corner (the next side's normal along / against the way you went)
    Vec3 rw, dw; screen_axes(&rw, &dw);
    Vec3 dWorld = sd == 0 ? vsc(dw, -1) : sd == 1 ? rw : sd == 2 ? dw : vsc(rw, -1);
    FFrame A = face_frame(rf), B = face_frame(f1);
    anim_start(AN_MOVE, 12); anSd = sd;
    anRel = vdot3(A.n, B.n) > 0.5f ? 0 : vdot3(B.n, dWorld) > 0.5f ? 1 : vdot3(B.n, dWorld) < -0.5f ? 2 : 0;
  }
  rf = f1; rrot = (d1 - sd) & 3;
  fold_to_screen(c1 % N, (c1 / N) % N, rrot, &rx, &ry);
  return 1;
}
static void sync_edge(int c) {                // an edge tile's doorway opens (or closes) on both sides
  int x = c % N, y = (c / N) % N;
  if (!(x == 0 || y == 0 || x == N - 1 || y == N - 1) || ((x == 0 || x == N - 1) && (y == 0 || y == N - 1))) return;
  int d = y == 0 ? 0 : x == N - 1 ? 1 : y == N - 1 ? 2 : 3, d1, m = fold_step(&S, c, d, &d1);
  if (m >= 0 && (sides[(m / NN) >> 1] >> ((m / NN) & 1) & 1)) { L.wall[m] = L.wall[c]; if (L.wall[m]) { L.box[m] = L.target[m] = L.btn[m] = L.kind[m] = L.aux[m] = 0; } }
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
    case 18:                                  // a sign: words to read (X on it sets them); solid, read from beside it
      if (c == L.player) return;
      L.wall[c] = 0; L.box[c] = L.target[c] = 0;
      if (L.kind[c] == FK_SIGN) { L.kind[c] = FK_FLOOR; L.btn[c] = L.drv[c] = L.aux[c] = L.txt[c] = 0; }
      else { L.kind[c] = FK_SIGN; L.btn[c] = L.drv[c] = L.aux[c] = 0; say("X ON IT: ITS WORDS"); }
      break;
    case 19:                                  // a keypad: the right code sends its signal (X on it sets the code)
      if (c == L.player) return;
      L.wall[c] = 0; L.box[c] = L.target[c] = 0;
      if (L.kind[c] == FK_KEYPAD) { L.kind[c] = FK_FLOOR; L.btn[c] = L.drv[c] = L.aux[c] = 0; break; }
      if (L.nkp == FOLD_MAXKP) { say("TOO MANY KEYPADS"); return; }
      memset(&L.kp[L.nkp], 0, sizeof L.kp[0]); L.kind[c] = FK_KEYPAD; L.aux[c] = (uint8_t)++L.nkp; L.drv[c] = 0;
      L.btn[c] = (uint8_t)next_signal(); say("X ON IT: ITS CODE");
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
  if (edge && !corner) sync_edge(c);
}
static void room_turn(int dir) {             // turn the room on screen, the cursor staying on its tile
  int c = screen_to_cell(rx, ry);
  anim_start(AN_TURN, 8); anDir = dir;
  rrot = (rrot + (dir > 0 ? 3 : 1)) & 3;   // R: clockwise
  fold_to_screen(c % N, (c / N) % N, rrot, &rx, &ry);
}
static void room_flip(int dir) {             // the right stick up / down: turn the cell over to its other side
  int f = rf ^ 1, c = screen_to_cell(rx, ry);
  if (!(sides[rf >> 1] >> (f & 1) & 1)) { say(f & 1 ? "NO ROOM ON THE BACK: A IN THE LEVEL VIEW ADDS ONE" : "NO ROOM ON THE FRONT"); return; }
  Vec3 rw, dw; screen_axes(&rw, &dw);           // turned over top to bottom: screen right stays, screen down reverses
  anim_start(AN_FLIP, 14); anDir = dir;
  int was = rf, best = 0; float bs = -1e9f; rf = f;
  for (int k = 0; k < 4; k++) { rrot = k; Vec3 r2, d2; screen_axes(&r2, &d2); float sc = vdot3(r2, rw) - vdot3(d2, dw); if (sc > bs) { bs = sc; best = k; } }
  rrot = best; (void)was;
  int cc = f * NN + (c % NN); fold_to_screen(cc % N, (cc / N) % N, rrot, &rx, &ry);   // the same spot, seen from the other side
}
static void room_layer(int dir) {            // to the same side of the cell stacked one above (towards you) or below
  const FoldSquare *q = &L.sq[rf >> 1]; int sg = rf & 1 ? -1 : 1, o[3], u[3], v[3];
  for (int k = 0; k < 3; k++) { o[k] = q->o[k] + dir * sg * q->n[k]; u[k] = q->u[k]; v[k] = q->v[k]; }
  int j = find_sq(o, u, v), f;
  if (j < 0) { say(dir > 0 ? "NO CELL ABOVE" : "NO CELL BELOW"); return; }
  const FoldSquare *w = &L.sq[j];                 // the side facing the same way as this one
  f = 2 * j + ((w->n[0] * q->n[0] + w->n[1] * q->n[1] + w->n[2] * q->n[2]) * sg < 0);
  if (!(sides[j] >> (f & 1) & 1)) { say("NO ROOM ON THAT CELL'S SIDE"); return; }
  rf = f;
}
// ---- the on-screen keyboard: names, a sign's words, a keypad's code -------------------------------------------------
static const char *KROWS[5] = { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ.,!?", "0123456789", "" };   // row 4: SPACE DEL DONE
static char *kbBuf; static int kbMax, kbBack, kbX, kbY, kbTokens; static const char *kbTitle;   // kbTokens: a row of live values
static const char *KTOK[7] = { "MOVES", "PUSHES", "FLIPS", "TURNS", "KEYS", "BOXES", "LIGHTS" };
static int krow_w(int y) { return y < 4 ? 10 : y == 4 ? 3 : 7; }
static void keys_open(char *buf, int max, const char *title) { kbBuf = buf; kbMax = max; kbTitle = title; kbBack = mode; kbX = kbY = 0; kbTokens = 0; mode = M_KEYS; }
static void keys_frame(const Pad *p) {
  int d, n = (int)strlen(kbBuf);
  if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) {
    int sd = PADDIR[d], w = krow_w(kbY), rows = kbTokens ? 6 : 5;
    if (sd == 1) kbX = (kbX + 1) % w; else if (sd == 3) kbX = (kbX + w - 1) % w;
    else {                                    // up / down: keep roughly the same place across rows of other widths
      int ny = (kbY + (sd == 2 ? 1 : rows - 1)) % rows, wn = krow_w(ny);
      kbX = ((2 * kbX + 1) * wn) / (2 * w); kbY = ny;
    }
  }
  if (pressed(p, PAD_A)) {
    if (kbY == 5) { char tk[16]; snprintf(tk, sizeof tk, "{%s}", KTOK[kbX]); if (n + (int)strlen(tk) <= kbMax) strcat(kbBuf, tk); }
    else if (kbY < 4) { if (n < kbMax) { kbBuf[n] = KROWS[kbY][kbX]; kbBuf[n + 1] = 0; } }
    else if (kbX == 0) { if (n < kbMax && n) { kbBuf[n] = ' '; kbBuf[n + 1] = 0; } }
    else if (kbX == 1) { if (n) kbBuf[n - 1] = 0; }
    else mode = kbBack;
  }
  if (pressed(p, PAD_B) && n) kbBuf[n - 1] = 0;
  if (pressed(p, PAD_START)) mode = kbBack;
  if (mode != M_KEYS) { n = (int)strlen(kbBuf); while (n && kbBuf[n - 1] == ' ') kbBuf[--n] = 0; }   // no trailing spaces
}
static void hints(const char **pairs, int n);
static void keys_draw(void) {
  gfx_dim(0, 0, GW, GH, 0.5f);
  int rows = kbTokens ? 6 : 5, top = kbTokens ? 70 : 110;
  gfx_panel(100, top, 440, 290 + (kbTokens ? 40 : 0), kbTitle, C_HI);
  char line[48]; int n = (int)strlen(kbBuf); const char *tail = n > 30 ? kbBuf + n - 30 : kbBuf;   // the last 30 typed
  snprintf(line, sizeof line, "%s%s", tail, (t >> 3) & 1 ? "_" : " ");
  gfx_rect(120, top + 46, 400, 30, 0x0a120a); gfx_text5(130, top + 54, 2, 0xe8ffd8, line);
  for (int y = 0; y < rows; y++) {
    int w = krow_w(y);
    for (int x = 0; x < w; x++) {
      int bw = y < 4 ? 36 : y == 4 ? 124 : 52, bx = 128 + x * (y < 4 ? 39 : y == 4 ? 130 : 55), by = top + 92 + y * 38, sel = x == kbX && y == kbY;
      gfx_rect(bx, by, bw, 32, sel ? 0x2f5a24 : 0x141c12); if (sel) gfx_frame(bx, by, bw, 32, C_HI);
      char k[8]; if (y < 4) { k[0] = KROWS[y][x]; k[1] = 0; } else if (y == 4) snprintf(k, sizeof k, "%s", x == 0 ? "SPACE" : x == 1 ? "DEL" : "DONE"); else snprintf(k, sizeof k, "%s", KTOK[x]);
      int sc = y == 5 ? 1 : 2;
      gfx_text5(bx + bw / 2 - gfx_text5_w(sc, k) / 2, by + (sc == 2 ? 9 : 12), sc, y == 5 ? 0xf2e01c : sel ? 0xe8ffd8 : C_TEXT, k);
    }
  }
  const char *h[] = { "DPAD", "KEY", "A", "TYPE", "B", "DELETE", "START", "DONE" }; hints(h, 4);
}
static void room_frame(const Pad *p) {
  int d;
  if (anKind && ++anT >= anLen) anKind = AN_NONE;
  if (inPal) {                                // on the tile palette (4 a row): the D-pad picks, A takes it, B goes back
    if (dpad_rep(p, &d)) {
      int sd = PADDIR[d];
      if (sd == 1 && item + 1 < NITEMS && (item + 1) % 4) item++;
      else if (sd == 3) { if (item % 4) item--; else inPal = 0; }    // left of the first column: back to the room
      else if (sd == 0 && item >= 4) item -= 4; else if (sd == 2 && item + 4 < NITEMS) item += 4;
    }
    if (pressed(p, PAD_A)) { inPal = 0; say(ITEMS[item]); }
    if (pressed(p, PAD_B)) { inPal = 0; item = palFrom; }
    if (pressed(p, PAD_START)) { mrow = 0; mode = M_MENU; }
    return;
  }
  if (dpad_rep(p, &d)) {                      // the D-pad moves the cursor: off the right edge onto the tile palette,
    int sd = PADDIR[d], nx = rx + (int)SX[sd], ny = ry + (int)SY[sd];   // off the others into the next cell
    if (nx >= 0 && ny >= 0 && nx < N && ny < N) { rx = nx; ry = ny; }
    else if (sd == 1) { inPal = 1; palFrom = item; }
    else go_room(screen_to_cell(rx, ry), sd);
  }
  if (stick_rep(p->lx, p->ly, &d)) {          // the left stick jumps a whole cell: from the edge tile that way
    int sd = PADDIR[d], ex = sd == 1 ? N - 1 : sd == 3 ? 0 : rx, ey = sd == 2 ? N - 1 : sd == 0 ? 0 : ry;
    if (!go_room(screen_to_cell(ex, ey), sd)) say("NO CELL THAT WAY");
  }
  if (rstick_rep(p->rx, p->ry, &d)) {         // the right stick: left / right turn the room a quarter, up / down turn
    int sd = PADDIR[d];                        // the cell over to its other side
    if (sd == 1) room_turn(1); else if (sd == 3) room_turn(-1); else room_flip(sd == 0 ? 1 : -1);
  }
  if (pressed(p, PAD_WHITE)) room_layer(1);   // White / Black: the room a layer above / below (out of / into the screen)
  if (pressed(p, PAD_BLACK)) room_layer(-1);
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
enum { P_NAME, P_MOVE, P_SIGNAL, P_TURNMODE, P_MIRX, P_MIRY, P_TURNLAY, P_COPY, P_PASTE, P_DELETE };
// layout tools (editing only, not the cell's moves in play): mirror, turn, copy and paste a side's room, as it lies
// on screen (propRot: the editor's turn, or the level view's when opened from there)
static struct { uint8_t a[8][NN]; int player, ok; } clip;
static uint8_t *layer(int k) { uint8_t *t[8] = { L.wall, L.target, L.box, L.btn, L.kind, L.aux, L.drv, L.txt }; return t[k]; }
static int face_cell(int f, int rot, int sx, int sy) {   // the tile of side f shown at screen (sx, sy)
  for (int c = f * NN; c < (f + 1) * NN; c++) { int x, y; fold_to_screen(c % N, (c / N) % N, rot, &x, &y); if (x == sx && y == sy) return c; }
  return f * NN;
}
static void layout_read(int f, int rot, uint8_t a[8][NN], int *player) {   // side f's tiles in screen order
  *player = -1;
  for (int sy = 0; sy < N; sy++) for (int sx = 0; sx < N; sx++) {
    int c = face_cell(f, rot, sx, sy);
    for (int k = 0; k < 8; k++) a[k][sy * N + sx] = layer(k)[c];
    if (L.kind[c] == FK_ONEWAY) a[5][sy * N + sx] = (uint8_t)((L.aux[c] - rot) & 3);   // a one-way's way, on screen
    if (c == L.player) *player = sy * N + sx;
  }
}
static void layout_write(int f, int rot, uint8_t a[8][NN], int player) {
  if (L.player / NN == f) L.player = -1;
  for (int sy = 0; sy < N; sy++) for (int sx = 0; sx < N; sx++) {
    int c = face_cell(f, rot, sx, sy), i = sy * N + sx;
    for (int k = 0; k < 8; k++) layer(k)[c] = a[k][i];
    if (L.kind[c] == FK_ONEWAY) L.aux[c] = (uint8_t)((a[5][i] + rot) & 3);
    if (i == player && !L.wall[c]) L.player = c;
  }
  for (int c = f * NN; c < (f + 1) * NN; c++) sync_edge(c);
}
static void layout_op(int op) {                 // 0 mirror left / right, 1 mirror up / down, 2 a quarter turn clockwise
  uint8_t a[8][NN], b[8][NN]; int pl, npl = -1;
  layout_read(propFace, propRot, a, &pl);
  for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
    int sx = op == 0 ? N - 1 - x : op == 1 ? x : y, sy = op == 0 ? y : op == 1 ? N - 1 - y : N - 1 - x, i = y * N + x, j = sy * N + sx;
    for (int k = 0; k < 8; k++) b[k][i] = a[k][j];
    if (b[4][i] == FK_ONEWAY) { int d = b[5][i]; b[5][i] = (uint8_t)(op == 0 ? (d == 1 ? 3 : d == 3 ? 1 : d) : op == 1 ? (d == 0 ? 2 : d == 2 ? 0 : d) : (d + 1) & 3); }
    if (j == pl) npl = i;
  }
  layout_write(propFace, propRot, b, npl);
}

static int prop_rows(int i, int *rows) {      // which rows the cell settings show
  int n = 0; rows[n++] = P_NAME; rows[n++] = P_MOVE; rows[n++] = P_SIGNAL;
  if (func[i].kind == 'r' || func[i].kind == 'R') rows[n++] = P_TURNMODE;
  if (propFace >> 1 == i && (sides[i] >> (propFace & 1) & 1)) {   // the layout tools, for the side with a room
    rows[n++] = P_MIRX; rows[n++] = P_MIRY; rows[n++] = P_TURNLAY; rows[n++] = P_COPY; if (clip.ok) rows[n++] = P_PASTE;
  }
  rows[n++] = P_DELETE;
  return n;
}
static void props_frame(const Pad *p) {
  int d, i = propSq, rows[12], nr = prop_rows(i, rows);
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
  if (pressed(p, PAD_A)) switch (rows[prow]) {
    case P_NAME: keys_open(L.sqname[i], 15, "NAME THIS CELL"); return;
    case P_MIRX: layout_op(0); say("LAYOUT MIRRORED LEFT TO RIGHT"); break;
    case P_MIRY: layout_op(1); say("LAYOUT MIRRORED TOP TO BOTTOM"); break;
    case P_TURNLAY: layout_op(2); say("LAYOUT TURNED"); break;
    case P_COPY: layout_read(propFace, propRot, clip.a, &clip.player); clip.ok = 1; say("LAYOUT COPIED"); break;
    case P_PASTE: layout_write(propFace, propRot, clip.a, clip.player); say("LAYOUT PASTED"); break;
  }
  if (pressed(p, PAD_B) || pressed(p, PAD_X)) mode = propsBack;
}

// ---- tile settings (Y on a tile in the cell editor) ---------------------------------------------------------------
enum { T_WORDS, T_WORDS2, T_KSRC, T_KARG, T_KOP, T_KN, T_KCMP, T_SENDS, T_WORKED, T_STARTS, T_LOOKS, T_CELL };
static int sends_ok(int c) { return L.kind[c] == FK_KEYPAD || L.kind[c] == FK_SIGN || L.kind[c] == FK_PLATE || L.kind[c] == FK_SENSOR || L.kind[c] == FK_KEY || L.kind[c] == FK_LIGHT || L.target[c] || (L.btn[c] && L.kind[c] == FK_FLOOR); }
static int worked_ok(int c) { return L.kind[c] == FK_SIGN || L.kind[c] == FK_GATE || L.kind[c] == FK_TELE || L.kind[c] == FK_ONEWAY || L.kind[c] == FK_LIGHT; }
static char kpNum[8]; static int kpNumFor = -1, kpNumField;   // a keypad number being typed (entry, 0 arg / 1 n)
static int tile_rows(int c, int *rows) {
  int n = 0;
  if (L.kind[c] == FK_SIGN) { rows[n++] = T_WORDS; rows[n++] = T_WORDS2; }
  if (L.kind[c] == FK_KEYPAD && L.aux[c] && L.aux[c] <= L.nkp) {
    const FoldKeypad *k = &L.kp[L.aux[c] - 1];
    rows[n++] = T_KSRC; if (k->src == FV_NUMBER || k->src == FV_COUNT) rows[n++] = T_KARG;
    rows[n++] = T_KOP; if (k->op) rows[n++] = T_KN; rows[n++] = T_KCMP;
  }
  if (sends_ok(c)) rows[n++] = T_SENDS;
  if (worked_ok(c)) rows[n++] = T_WORKED;
  if (L.kind[c] == FK_GATE) { rows[n++] = T_STARTS; rows[n++] = T_LOOKS; }
  if (L.kind[c] == FK_TELE || L.kind[c] == FK_LIGHT) rows[n++] = T_STARTS;
  rows[n++] = T_CELL;
  return n;
}
static void tile_frame(const Pad *p) {
  int d, c = tcell, rows[12], nr = tile_rows(c, rows);
  if (kpNumFor >= 0) {                        // back from typing a keypad's number
    if (kpNum[0]) { if (kpNumField) L.kp[kpNumFor].n = (int16_t)atoi(kpNum); else L.kp[kpNumFor].arg = (int16_t)atoi(kpNum); }
    kpNumFor = -1;
  }
  if (nr == 1) { propSq = rf >> 1; propFace = rf; propRot = rrot; prow = 0; propsBack = M_ROOM; mode = M_PROPS; return; }   // nothing to set: the cell's
  if (trow >= nr) trow = nr - 1;
  if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) {
    int sd = PADDIR[d];
    if (sd == 0) trow = (trow + nr - 1) % nr; else if (sd == 2) trow = (trow + 1) % nr;
    else {
      int st = sd == 1 ? 1 : -1, r = rows[trow];
      int needs = L.kind[c] == FK_PLATE || L.kind[c] == FK_SENSOR || (L.btn[c] && L.kind[c] == FK_FLOOR && !L.target[c]);
      if (r == T_SENDS) { int k = sig_cycle(L.btn[c], st, !needs); L.btn[c] = (uint8_t)(needs && !k ? next_signal() : k); }
      else if (r == T_WORKED) L.drv[c] = (uint8_t)sig_cycle(L.drv[c], st, 1);
      else if (r >= T_KSRC && r <= T_KCMP) {   // a keypad's code
        FoldKeypad *k = &L.kp[L.aux[c] - 1];
        if (r == T_KSRC) k->src = (uint8_t)((k->src + st + FV_N) % FV_N);
        else if (r == T_KARG) { if (k->src == FV_COUNT) k->arg = (int16_t)sig_cycle(k->arg, st, 0); else k->arg = (int16_t)(k->arg + st); }
        else if (r == T_KOP) k->op = (uint8_t)((k->op + st + 4) % 4);
        else if (r == T_KN) k->n = (int16_t)(k->n + st);
        else k->cmp = (uint8_t)((k->cmp + st + 3) % 3);
      }
      else if (r == T_STARTS) L.aux[c] ^= 0x80;   // a gate open / shut, a teleporter off / on
      else if (r == T_LOOKS) L.aux[c] ^= 0x40;
    }
  }
  if (pressed(p, PAD_A) && (rows[trow] == T_WORDS || rows[trow] == T_WORDS2)) {   // a sign's words: the keyboard
    int two = rows[trow] == T_WORDS2, k = two ? L.aux[c] : L.txt[c];
    if (!k) { if (L.ntext == FOLD_MAXTEXT) { say("TOO MANY TEXTS IN THIS LEVEL"); return; } L.text[L.ntext][0] = 0; k = ++L.ntext; if (two) L.aux[c] = (uint8_t)k; else L.txt[c] = (uint8_t)k; }
    keys_open(L.text[k - 1], FOLD_TEXTLEN - 1, two ? "ITS WORDS WHILE ITS SIGNAL IS ON" : "THE SIGN SAYS"); kbTokens = 1;
    return;
  }
  if (pressed(p, PAD_A) && (rows[trow] == T_KARG || rows[trow] == T_KN) && L.aux[c]) {   // type the number
    FoldKeypad *k = &L.kp[L.aux[c] - 1];
    if (rows[trow] == T_KARG && k->src == FV_COUNT) return;
    kpNumFor = L.aux[c] - 1; kpNumField = rows[trow] == T_KN; kpNum[0] = 0;
    keys_open(kpNum, 6, kpNumField ? "BY HOW MUCH" : "THE CODE"); return;
  }
  if (pressed(p, PAD_A) && rows[trow] == T_CELL) { propSq = rf >> 1; propFace = rf; propRot = rrot; prow = 0; propsBack = M_ROOM; mode = M_PROPS; return; }
  if (pressed(p, PAD_B) || pressed(p, PAD_X)) mode = M_ROOM;
}

// ---- wiring: the logic as a picture (owner: physical wires and gates) --------------------------------------------
// Every thing that sends a signal is a node on the left (tiles, timers, "all targets filled"), the logic pieces are
// gates in the middle, laid out by what feeds them, and what signals work is on the right (gates, teleporters, one-ways,
// lights, signs, cells with a move). A wire runs from an output to each input on the same signal; the numbers stay
// hidden: picking up an output (A) and putting it down on an input (A) sets them.
static const char *OPNAMES[] = { "AND", "OR", "XOR", "NOT", "LATCH", "COUNT", "DELAY", "SHIFT" };
#define NOPS 8
enum { NK_ALL, NK_TIMER, NK_SRC, NK_LOGIC, NK_SINK, NK_CELL };
typedef struct { int kind, ref, x, y, w, h; } WNode;
static WNode wn[200]; static int nwn, wCols;   // wCols: the number of columns (the last: what is worked)
static int wcur, wport, wHold = -1, wAdd = -1, wScX, wScY;   // cursor node / port (-1 its output, 0.. inputs); the wire carried
static int lop_arity(int op) { return op == FL_NOT || op == FL_COUNT || op == FL_DELAY ? 1 : op == FL_LATCH || op == FL_SHIFT ? 2 : 0; }   // 0: 2..4
static int node_out(const WNode *n) {           // the signal it sends (-1: no output)
  switch (n->kind) { case NK_ALL: return L.allSig; case NK_TIMER: return L.timer[n->ref].sig; case NK_SRC: return L.btn[n->ref];
                     case NK_LOGIC: return L.logic[n->ref].out; }
  return -1;
}
static int node_nin(const WNode *n) {
  if (n->kind == NK_SINK || n->kind == NK_CELL) return 1;
  if (n->kind != NK_LOGIC) return 0;
  const FoldLogic *q = &L.logic[n->ref]; int a = lop_arity(q->op);
  if (a) return a;
  int used = 0; for (int i = 0; i < 4; i++) if (q->in[i]) used = i + 1;
  return used < 2 ? 2 : used < 4 ? used + 1 : 4;   // an empty input to wire into, up to 4
}
static int node_in(const WNode *n, int i) {
  switch (n->kind) { case NK_SINK: return L.drv[n->ref]; case NK_CELL: return trig[n->ref]; case NK_LOGIC: return L.logic[n->ref].in[i]; }
  return 0;
}
static void set_out(const WNode *n, int s) {
  switch (n->kind) { case NK_ALL: L.allSig = s; break; case NK_TIMER: L.timer[n->ref].sig = (uint8_t)s; break;
                     case NK_SRC: L.btn[n->ref] = (uint8_t)s; break; case NK_LOGIC: L.logic[n->ref].out = (uint8_t)s; break; }
}
static void set_in(const WNode *n, int i, int s) {
  if (n->kind == NK_SINK) L.drv[n->ref] = (uint8_t)s;
  else if (n->kind == NK_CELL) trig[n->ref] = s;
  else if (n->kind == NK_LOGIC) {
    FoldLogic *q = &L.logic[n->ref]; int a = lop_arity(q->op);
    q->in[i] = (uint8_t)s;
    if (!a) { int k = 0; for (int j = 0; j < 4; j++) if (q->in[j]) q->in[k++] = q->in[j]; while (k < 4) q->in[k++] = 0; }   // no gaps (a gap reads off)
    int used = 0; for (int k = 0; k < 4; k++) if (q->in[k]) used = k + 1;
    q->n = (uint8_t)(a ? a : used);
  }
}
static const char *src_kind(int c) {
  return L.kind[c] == FK_PLATE ? "PLATE" : L.kind[c] == FK_SENSOR ? "SENSOR" : L.kind[c] == FK_KEY ? "KEY" : L.kind[c] == FK_LIGHT ? "LIGHT" :
         L.kind[c] == FK_SIGN ? "SIGN" : L.kind[c] == FK_KEYPAD ? "KEYPAD" : L.kind[c] == FK_GATE ? (L.aux[c] & 0x40 ? "WALL GATE" : "GATE") :
         L.kind[c] == FK_TELE ? "TELEPORTER" : L.kind[c] == FK_ONEWAY ? "ONE-WAY" : L.target[c] ? "TARGET" : "BUTTON";
}
static void where(int c, char *b, int n) {      // "KITCHEN" or "CELL 2", and B for a back side
  int i = c / NN / 2, back = (c / NN) & 1;
  if (L.sqname[i][0]) snprintf(b, (size_t)n, "%s%s", L.sqname[i], back ? " BACK" : ""); else snprintf(b, (size_t)n, "CELL %d%s", i, back ? " BACK" : "");
}
static void wire_build(void) {                  // the nodes and where they go (columns by what feeds what)
  nwn = 0;
  wn[nwn++] = (WNode){ NK_ALL, 0, 0, 0, 0, 0 };
  for (int k = 0; k < L.ntimer && nwn < 200; k++) wn[nwn++] = (WNode){ NK_TIMER, k, 0, 0, 0, 0 };
  for (int c = 0; c < 2 * L.nsq * NN && nwn < 200; c++)
    if (!L.wall[c] && (sides[c / NN >> 1] >> ((c / NN) & 1) & 1) && sends_ok(c) && (L.btn[c] || !L.target[c])) wn[nwn++] = (WNode){ NK_SRC, c, 0, 0, 0, 0 };
  for (int k = 0; k < L.nlogic && nwn < 200; k++) wn[nwn++] = (WNode){ NK_LOGIC, k, 0, 0, 0, 0 };
  for (int c = 0; c < 2 * L.nsq * NN && nwn < 200; c++)
    if (!L.wall[c] && (sides[c / NN >> 1] >> ((c / NN) & 1) & 1) && worked_ok(c)) wn[nwn++] = (WNode){ NK_SINK, c, 0, 0, 0, 0 };
  for (int i = 0; i < L.nsq && nwn < 200; i++) if (func[i].kind) wn[nwn++] = (WNode){ NK_CELL, i, 0, 0, 0, 0 };
  int sd[FOLD_MAXSIG], depth[200], maxd = 0;    // a signal's column: its senders' furthest
  for (int s2 = 0; s2 < FOLD_MAXSIG; s2++) sd[s2] = 0;
  for (int pass = 0; pass < 8; pass++)
    for (int k = 0; k < L.nlogic; k++) {
      const FoldLogic *q = &L.logic[k]; int d = 0;
      for (int i = 0; i < 4; i++) if (q->in[i] && sd[q->in[i]] > d) d = sd[q->in[i]];
      int span = q->op == FL_SHIFT ? q->count : 1;
      for (int i = 0; i < span; i++) if (q->out + i < FOLD_MAXSIG && sd[q->out + i] < d + 1) sd[q->out + i] = d + 1 > 8 ? 8 : d + 1;
    }
  for (int k = 0; k < nwn; k++) {
    WNode *n = &wn[k];
    if (n->kind == NK_LOGIC) { const FoldLogic *q = &L.logic[n->ref]; int d = 0; for (int i = 0; i < 4; i++) if (q->in[i] && sd[q->in[i]] > d) d = sd[q->in[i]]; depth[k] = d + 1; }
    else depth[k] = n->kind == NK_SINK || n->kind == NK_CELL ? -1 : 0;
    if (depth[k] > maxd) maxd = depth[k];
  }
  int colY[12]; for (int k = 0; k < 12; k++) colY[k] = 0;
  wCols = maxd + 2;
  for (int k = 0; k < nwn; k++) {
    WNode *n = &wn[k]; int d = depth[k] < 0 ? maxd + 1 : depth[k];
    n->w = 128; n->h = 30 + (node_nin(n) > 1 ? (node_nin(n) - 1) * 14 : 0);
    n->x = 18 + d * 170; n->y = colY[d]; colY[d] += n->h + 12;
  }
}
static void port_xy(const WNode *n, int port, int *x, int *y) {   // on screen (scrolled)
  *x = n->x + (port < 0 ? n->w : 0) - wScX; *y = n->y + 22 + (port < 0 ? 0 : port * 14) - wScY + 70;
}
static void wire_frame(const Pad *p) {
  int d;
  wire_build();
  if (wcur >= nwn) wcur = 0;
  if (wAdd >= 0) {                             // the add list: gates, then timers
    if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) { int sd = PADDIR[d]; if (sd == 0) wAdd = (wAdd + NOPS + 1) % (NOPS + 2); else if (sd == 2) wAdd = (wAdd + 1) % (NOPS + 2); }
    if (pressed(p, PAD_A)) {
      if (wAdd < NOPS && L.nlogic < FOLD_MAXLOGIC) {
        FoldLogic *q = &L.logic[L.nlogic]; memset(q, 0, sizeof *q); q->op = (uint8_t)wAdd; q->count = wAdd == FL_SHIFT ? 4 : 2; q->n = (uint8_t)lop_arity(q->op);
        q->out = (uint8_t)next_signal(); L.nlogic++; say("A GATE: WIRE INTO IT");
      } else if (wAdd >= NOPS && L.ntimer < FOLD_MAXTIMER) {
        FoldTimer *q = &L.timer[L.ntimer]; q->kind = wAdd == NOPS ? FT_TICK : FT_MOVES; q->n = wAdd == NOPS ? 2 : 20; q->sig = (uint8_t)next_signal(); L.ntimer++; say("A TIMER");
      }
      wAdd = -1;
    }
    if (pressed(p, PAD_B) || pressed(p, PAD_Y)) wAdd = -1;
    return;
  }
  WNode *n = &wn[wcur];
  if (wport >= node_nin(n) || (wport < 0 && node_out(n) < 0)) wport = node_out(n) >= 0 ? -1 : 0;
  if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) {   // left / right: the next column of ports, the nearest
    int sd = PADDIR[d], cx, cy, best = -1, bp = 0;          // by height; up / down: along the column
    float bs = 1e9f, nearest = 1e9f;
    port_xy(n, wport, &cx, &cy);
    for (int pass = 0; pass < 2; pass++)
      for (int k = 0; k < nwn; k++) for (int pp = -1; pp < node_nin(&wn[k]); pp++) {
        if (pp < 0 && (node_out(&wn[k]) < 0 || wHold > 0)) continue;   // carrying a wire: only inputs
        if (k == wcur && pp == wport) continue;
        int x, y; port_xy(&wn[k], pp, &x, &y);
        float dx = (float)(x - cx), dy = (float)(y - cy), along = sd == 0 ? -dy : sd == 1 ? dx : sd == 2 ? dy : -dx, across = sd == 0 || sd == 2 ? fabsf(dx) : fabsf(dy);
        if (along < 3) continue;
        if (pass == 0) { if ((sd == 1 || sd == 3) ? along < nearest : 0) nearest = along; continue; }   // the next column over
        float sc;
        if (sd == 1 || sd == 3) { if (along > nearest + 20) continue; sc = across; }
        else sc = across <= 20 ? along : along + 4 * across + 1000;   // the same column first
        if (sc < bs) { bs = sc; best = k; bp = pp; }
      }
    if (best >= 0) { wcur = best; wport = bp; n = &wn[wcur]; }
  }
  if (pressed(p, PAD_A)) {
    if (wport < 0) {                           // pick up this output's wire (a new signal if it has none)
      int s2 = node_out(n); if (!s2) { s2 = next_signal(); set_out(n, s2); }
      wHold = s2;
    } else if (wHold > 0) { set_in(n, wport, wHold); wHold = -1; say("WIRED"); }
    else if (node_in(n, wport)) { wHold = node_in(n, wport); set_in(n, wport, 0); }   // pick the wire's end up again
  }
  if (pressed(p, PAD_X) && wHold < 0) {
    if (wport >= 0 && node_in(n, wport)) { set_in(n, wport, 0); say("WIRE CUT"); }
    else if (wport < 0 && n->kind == NK_LOGIC) { int k = n->ref; memmove(&L.logic[k], &L.logic[k + 1], sizeof(FoldLogic) * (size_t)(L.nlogic - k - 1)); L.nlogic--; say("GATE REMOVED"); wcur = 0; }
    else if (wport < 0 && n->kind == NK_TIMER) { int k = n->ref; memmove(&L.timer[k], &L.timer[k + 1], sizeof(FoldTimer) * (size_t)(L.ntimer - k - 1)); L.ntimer--; say("TIMER REMOVED"); wcur = 0; }
  }
  if (n->kind == NK_LOGIC && (pressed(p, PAD_L) || pressed(p, PAD_R))) {   // L / R: the gate's kind
    FoldLogic *q = &L.logic[n->ref]; q->op = (uint8_t)((q->op + (pressed(p, PAD_R) ? 1 : NOPS - 1)) % NOPS);
    int a = lop_arity(q->op); if (a) { q->n = (uint8_t)a; for (int i = a; i < 4; i++) q->in[i] = 0; }
    if ((q->op == FL_DELAY || q->op == FL_SHIFT) && q->count > 8) q->count = 8;
  }
  if (n->kind == NK_TIMER && (pressed(p, PAD_L) || pressed(p, PAD_R))) L.timer[n->ref].kind ^= 1;
  if (rstick_rep(p->rx, p->ry, &d)) {           // the right stick: its number (a counter's, a delay's, a register's, a timer's)
    int sd = PADDIR[d], st = sd == 1 || sd == 0 ? 1 : -1;
    if (n->kind == NK_LOGIC) { FoldLogic *q = &L.logic[n->ref]; int mx = q->op == FL_COUNT ? 99 : 8, c2 = q->count + st; q->count = (uint8_t)(c2 < 1 ? 1 : c2 > mx ? mx : c2); }
    if (n->kind == NK_TIMER) { FoldTimer *q = &L.timer[n->ref]; int c2 = q->n + st; q->n = (uint8_t)(c2 < 1 ? 1 : c2 > 255 ? 255 : c2); }
  }
  if (pressed(p, PAD_Y)) wAdd = 0;
  if (pressed(p, PAD_B)) { if (wHold > 0) wHold = -1; else mode = M_SHAPE; }
  if (pressed(p, PAD_START)) { wHold = -1; mode = M_SHAPE; }
  int x, y; port_xy(n, wport, &x, &y);          // keep the cursor in view
  if (x < 60) wScX -= 60 - x; if (x > GW - 60) wScX += x - (GW - 60); if (y < 110) wScY -= 110 - y; if (y > GH - 70) wScY += y - (GH - 70);
  if (wScX < 0) wScX = 0; if (wScY < 0) wScY = 0;
}

// ---- menu and playtest -------------------------------------------------------------------------------------------
static const char *MENU[] = { "PLAYTEST", "VMX WINDOW", "WIRING", "SAVE", "LOAD", "NEW LEVEL", "QUIT" };
#define NMENU 7
static char files[64][64]; static int nfiles;
static void start_play(void) {
  build(&PL);
  if (PL.player < 0) { say("PUT THE PLAYER IN A CELL FIRST"); mode = M_SHAPE; return; }
  fold_start(&G, &PL); popT = wonT = 0; bannerSq = -1; kpOn = 0; mode = M_PLAY;
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
      case 2: wcur = 0; wport = -1; wHold = -1; wAdd = -1; mode = M_WIRE; return;
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
  if (kpOn) {                                 // at a keypad: the D-pad picks a key, A presses it, B leaves
    int d;
    if (kpMsgT) { if (--kpMsgT == 0 && kpMsg && kpMsg[0] == 'R') kpOn = 0; return; }
    if (dpad_rep(p, &d) || stick_rep(p->lx, p->ly, &d)) {
      int sd = PADDIR[d], x = kpSel % 3, y = kpSel / 3;
      if (sd == 1) x = (x + 1) % 3; else if (sd == 3) x = (x + 2) % 3; else if (sd == 0) y = (y + 3) % 4; else y = (y + 1) % 4;
      kpSel = y * 3 + x;
    }
    if (pressed(p, PAD_A)) {
      int n = (int)strlen(kpBuf);
      if (kpSel < 9 || kpSel == 10) { if (n < 6) { kpBuf[n] = (char)(kpSel == 10 ? '0' : '1' + kpSel); kpBuf[n + 1] = 0; } }
      else if (kpSel == 9) { if (n) kpBuf[n - 1] = 0; }
      else if (n) { int r = fold_keypad(&G, kpCell, atoi(kpBuf)); kpMsg = r ? "RIGHT" : "WRONG"; kpMsgT = 30; kpBuf[0] = 0; }
    }
    if (pressed(p, PAD_B) || pressed(p, PAD_START)) kpOn = 0;
    return;
  }
  { int sq = G.p >= 0 ? G.p / NN / 2 : -1;     // walking into a named cell shows its name for a moment
    if (sq != bannerSq) { bannerSq = sq; bannerT = sq >= 0 && PL.sqname[sq][0] ? 45 : 0; } else if (bannerT) bannerT--; }
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
  if (pressed(p, PAD_A)) {                    // talk to the tile you face (or the one you stand on); reading a sign may send
    int c = fold_facing(&G);
    if (c >= 0 && G.kind[c] == FK_SIGN) fold_read(&G, c);
    if (c >= 0 && G.kind[c] == FK_KEYPAD) { kpOn = 1; kpCell = c; kpSel = 0; kpBuf[0] = 0; kpMsgT = 0; return; }
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
  return snprintf(b, (size_t)n, "m%d r%d,%d,%d,%d p%d w%d,%d i%d c%d s%d l%d,%d", mode, rx, ry, rf, rrot, mrow, wcur, wport, item, L.nsq, L.player, L.nlogic, L.logic[0].op);
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
    case M_KEYS: keys_frame(p); break;
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
    } else if (L.kind[c] == FK_SIGN) {         // a sign: a post with a wooden board on top
      static uint32_t woodPx[1] = { 0xc8a468 }, postPx[1] = { 0x5a4428 };
      Tex wood = { 1, 1, woodPx }, post = { 1, 1, postPx };
      block(vadd(p, vadd(vsc(du, 0.44f), vsc(dv, 0.44f))), vsc(du, 0.12f), vsc(dv, 0.12f), n, cs * 0.7f, &post, &post, NULL);
      block(vadd(vadd(p, vadd(vsc(du, 0.12f), vsc(dv, 0.42f))), vsc(n, cs * 0.55f)), vsc(du, 0.76f), vsc(dv, 0.16f), n, cs * 0.6f, &wood, &wood, NULL);
    } else if (L.kind[c] == FK_KEYPAD) {       // a keypad: a grey panel of keys on a post
      static uint32_t padPx[25], postPx[1] = { 0x3a3e46 }; static int pk;
      if (!pk) { pk = 1; for (int i = 0; i < 25; i++) padPx[i] = ((i % 5) & 1) && ((i / 5) & 1) ? 0xe8ecf4 : 0x6a7080; }
      Tex pad = { 5, 5, padPx }, post = { 1, 1, postPx };
      block(vadd(p, vadd(vsc(du, 0.44f), vsc(dv, 0.44f))), vsc(du, 0.12f), vsc(dv, 0.12f), n, cs * 0.7f, &post, &post, NULL);
      block(vadd(vadd(p, vadd(vsc(du, 0.2f), vsc(dv, 0.2f))), vsc(n, cs * 0.6f)), vsc(du, 0.6f), vsc(dv, 0.6f), n, cs * 0.25f, &pad, &pad, NULL);
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
static void draw_ground(void) {               // a soft grid fading into the distance (no shadows: owner)
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
               case 14: return 0xa0a8a0; case 17: return 0xfff27a; case 18: return 0xd8b878; case 19: return 0xa8b0c8; }
  return 0x7dff3a;
}
// the whole level, small, under the tile palette: it follows the room being edited (yellow), seen from above it and
// turned as the editor shows it, gliding to the next room when the cursor walks or jumps there
static Vec3 mlEye, mlAt, mlUp; static int mlInit;
static void screen_axes(Vec3 *rw, Vec3 *dw) {   // the cell editor's screen right / down in the world (room x, y turned by rrot)
  FFrame F = face_frame(rf); int x0, y0, x1, y1, x2, y2;
  fold_to_screen(0, 0, rrot, &x0, &y0); fold_to_screen(1, 0, rrot, &x1, &y1); fold_to_screen(0, 1, rrot, &x2, &y2);
  *rw = vadd(vsc(F.u, (float)(x1 - x0)), vsc(F.v, (float)(x2 - x0)));
  *dw = vadd(vsc(F.u, (float)(y1 - y0)), vsc(F.v, (float)(y2 - y0)));
}
static const char *compass_letter(Vec3 d) {     // a whole axis as a direction: E / W (x), N / S (y), U / D (z)
  return d.x > 0.5f ? "E" : d.x < -0.5f ? "W" : d.y > 0.5f ? "N" : d.y < -0.5f ? "S" : d.z > 0.5f ? "U" : "D";
}
static void mini_level(int x, int y, int w, int h) {
  if (!L.nsq) return;
  FFrame F = face_frame(rf);
  Vec3 c = vadd(F.o, vsc(vadd(F.u, F.v), 0.5f));
  Vec3 rw, dw; screen_axes(&rw, &dw);
  const float dist = 4.2f, cp = cosf(0.75f), sp = sinf(0.75f), span = 3.6f;
  Vec3 eyeG = vadd(c, vadd(vsc(F.n, dist * sp), vsc(dw, dist * cp))), upG = vadd(vsc(dw, -sp), vsc(F.n, cp));
  Vec3 dOld = vnormz(vadd(mlEye, vsc(mlAt, -1))), dNew = vnormz(vadd(eyeG, vsc(c, -1)));
  if (!mlInit || vdot3(dOld, dNew) < 0.2f || vdot3(mlUp, upG) < -0.2f) { mlEye = eyeG; mlAt = c; mlUp = upG; mlInit = 1; }   // the other way round: jump
  mlEye = vmix(mlEye, eyeG, 0.2f); mlAt = vmix(mlAt, c, 0.2f); mlUp = vnormz(vmix(mlUp, upG, 0.2f));
  Vec3 e = mlEye;
  gfx_clip(x, y, w, h); gfx_zclear_rect(x, y, w, h); gfx_fog(0, 1e9f, 2e9f);
  gfx_view_offset(x + w / 2 - GW / 2, y + h / 2 - GH / 2);
  gfx_camera(mat_mul(mat_perspective(2 * atanf(span * GH / w / 2 / dist), (float)GW / GH, 0.05f, 200.0f), mat_lookat(e, mlAt, mlUp)));
  for (int i = 0; i < L.nsq; i++) {
    const FoldSquare *q = &L.sq[i];
    Vec3 o = v3f(q->o[0], q->o[1], q->o[2]), u = v3f(q->u[0], q->u[1], q->u[2]), v = v3f(q->v[0], q->v[1], q->v[2]);
    Vec3 P4[4] = { o, vadd(o, u), vadd(vadd(o, u), v), vadd(o, v) };
    Vec3 m = vadd(o, vsc(vadd(u, v), 0.5f));
    float fd = (e.x - m.x) * q->n[0] + (e.y - m.y) * q->n[1] + (e.z - m.z) * q->n[2];
    int side = fd < 0, f = 2 * i + side, room = sides[i] >> side & 1;   // the side facing the camera
    uint32_t col = f == rf ? 0xf2e01c : room ? (side ? 0x2f8a1c : 0x52c41a) : 0x2a302a;
    gfx_quad(P4, NULL, col, 1);
    for (int k = 0; k < 4; k++) gfx_line3(P4[k], P4[(k + 1) & 3], i == rf >> 1 ? 0xf2e01c : 0x0c140c, i == rf >> 1 ? 2 : 1);   // this cell: outlined
  }
  gfx_clip(0, 0, 0, 0); gfx_view_offset(0, 0);
}
static void room_anim_draw(int X0, int Y0) {    // the room as 3D quads while an animation runs (see anKind)
  float t = (anT + 1) / (float)anLen; if (t > 1) t = 1; t = t * t * (3 - 2 * t);
  room_picture(rf, rrot, anNew);
  Tex to = { 56, 56, anOld }, tn = { 56, 56, anNew };
  gfx_rect(X0, Y0, 336, 336, VMX_DARK);
  gfx_clip(X0, Y0, 336, 336); gfx_zclear_rect(X0, Y0, 336, 336); gfx_fog(0, 1e9f, 2e9f);
  gfx_view_offset(X0 + 168 - GW / 2, Y0 + 168 - GH / 2);
  const float E = 2.6f;
  gfx_camera(mat_mul(mat_perspective(2 * atanf(GH / 336.0f / 2 / E), (float)GW / GH, 0.05f, 50.0f), mat_lookat(v3f(0, 0, E), v3f(0, 0, 0), v3f(0, 1, 0))));
  static const float CX[4] = { -0.5f, 0.5f, 0.5f, -0.5f }, CY[4] = { 0.5f, 0.5f, -0.5f, -0.5f };   // TL TR BR BL
  Vec3 P[4];
  if (anKind == AN_TURN) {                     // the new picture, from the old turn to its own
    float a = (1 - t) * (anDir > 0 ? 90 : -90) * PI / 180, c = cosf(a), sn = sinf(a);
    for (int k = 0; k < 4; k++) P[k] = v3f(CX[k] * c - CY[k] * sn, CX[k] * sn + CY[k] * c, 0);
    gfx_quad(P, &tn, 0, 1);
  } else if (anKind == AN_FLIP) {              // turned over about the screen's horizontal axis: old, then new
    float th = t * PI * anDir, a = t < 0.5f ? th : th - PI * anDir, c = cosf(a), sn = sinf(a);
    for (int k = 0; k < 4; k++) P[k] = v3f(CX[k], CY[k] * c, CY[k] * sn);
    gfx_quad(P, t < 0.5f ? &to : &tn, 0, 1);
  } else {                                      // into the next cell: slide, or turn over the corner
    float dx = anSd == 1 ? 1.f : anSd == 3 ? -1.f : 0.f, dy = anSd == 0 ? 1.f : anSd == 2 ? -1.f : 0.f;
    for (int q = 0; q < 2; q++) {               // q 0: the old room, 1: the new one (its final place, moved back)
      float th = 0, cz = 0, off = 0;
      if (anRel == 0) off = q ? 1 - t : -t;
      else if (anRel == 1) { cz = -0.5f; th = (t * 90 - (q ? 90 : 0)) * PI / 180; }
      else { cz = 0.5f; th = (-t * 90 + (q ? 90 : 0)) * PI / 180; }
      for (int k = 0; k < 4; k++) {
        float sv = CX[k] * dx + CY[k] * dy, px = CX[k] - sv * dx, py = CY[k] - sv * dy, s2, z2;
        if (anRel == 0) { s2 = sv + off; z2 = 0; }
        else { s2 = sv * cosf(th) + cz * sinf(th); z2 = cz + sv * sinf(th) - cz * cosf(th); }
        P[k] = v3f(px + s2 * dx, py + s2 * dy, z2);
      }
      gfx_quad(P, q ? &tn : &to, 0, 1);
    }
  }
  gfx_clip(0, 0, 0, 0); gfx_view_offset(0, 0);
}
static void draw_room_editor(void) {
  memset(bits, 0, sizeof bits);
  vmx_room(bits, L.wall, L.target, L.box, L.btn, L.kind, L.aux, rf, rrot, L.player, &S, 0);
  const int S6 = 6, X0 = 30, Y0 = 62;          // the room 6x bigger: a cell is 42 px
  char s[64];
  if (L.sqname[rf >> 1][0]) snprintf(s, sizeof s, "%s  %s", L.sqname[rf >> 1], rf & 1 ? "BACK" : "FRONT");
  else snprintf(s, sizeof s, "CELL %d  %s SIDE", rf >> 1, rf & 1 ? "BACK" : "FRONT");
  gfx_panel(X0 - 12, Y0 - 40, 56 * S6 + 24, 56 * S6 + 52, s, C_HI);
  if (anKind) room_anim_draw(X0, Y0);
  else for (int y = 0; y < 56; y++) for (int x = 0; x < 56; x++) gfx_rect(X0 + x * S6, Y0 + y * S6, S6, S6, bits[(y + 8) * 64 + x + 4] ? VMX_LIT : VMX_DARK);
  uint32_t cc = (t >> 3) & 1 ? C_CURSOR : 0xffffff;
  if (!inPal && !anKind) for (int w = 0; w < 3; w++) gfx_frame(X0 + rx * 42 - w, Y0 + ry * 42 - w, 42 + 2 * w, 42 + 2 * w, cc);
  { Vec3 rw, dw; screen_axes(&rw, &dw);          // which way each edge faces in the world: N S E W, U up, D down
    const Vec3 dir[4] = { vsc(dw, -1), rw, dw, vsc(rw, -1) };
    const int ex[4] = { X0 + 168, X0 + 336, X0 + 168, X0 }, ey[4] = { Y0, Y0 + 168, Y0 + 336, Y0 + 168 };
    for (int k = 0; k < 4; k++) {
      gfx_disc(ex[k], ey[k], 12, 0x0a120a); gfx_ring(ex[k], ey[k], 12, 2, C_HI);
      gfx_text5(ex[k] - 5, ey[k] - 7, 2, 0xf2e01c, compass_letter(dir[k]));
    } }
  // the tile palette: icons in a grid, the picked one lit up and named
  int px0 = 410, py0 = 22, cols = 4;
  gfx_panel(px0, py0, 216, 300, "TILES", C_HI);
  for (int k = 0; k < NITEMS; k++) {
    int bx = px0 + 14 + (k % cols) * 48, by = py0 + 44 + (k / cols) * 46;
    if (k == item) { gfx_rect(bx - 4, by - 4, 44, 44, 0x2f5a24); gfx_frame(bx - 4, by - 4, 44, 44, C_HI); }
    if (k == item && inPal) for (int w = 0; w < 3; w++) gfx_frame(bx - 6 - w, by - 6 - w, 48 + 2 * w, 48 + 2 * w, (t >> 3) & 1 ? C_CURSOR : 0xffffff);
    const char **spr = vmx_icon(ITEMS[k]); uint32_t ic = item_colour(k);
    gfx_rect(bx, by, 36, 36, 0x0a120a);
    for (int j = 0; j < 7; j++) for (int i2 = 0; i2 < 7; i2++) if (spr[j][i2] == '#') gfx_rect(bx + 1 + i2 * 5, by + 1 + j * 5, 5, 5, ic);
  }
  gfx_text5(px0 + 14, py0 + 270, 2, C_HI, ITEMS[item]);
  gfx_hint(px0 + 4, py0 + 312, "DPAD", inPal ? "PICK   A TAKE   B BACK" : "RIGHT: TO THE TILES");
  gfx_dim(px0, 352, 216, 92, 0.5f); gfx_frame(px0, 352, 216, 92, 0x2f5a24);   // the whole level, small
  mini_level(px0 + 2, 354, 212, 88);
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
  if (mode == M_KEYS) { mode = kbBack; app_draw(); mode = M_KEYS; keys_draw(); return; }   // the keyboard over its screen
  gfx_gradient(0x142012, 0x040604);
  if (mode == M_PLAY) {
    static uint8_t b[64 * 64];
    vmx_game(b, &G, 1, popT ? popWord : NULL);
    if (bannerT && !popT) {                   // the cell's name across the top of the room
      const char *nm = PL.sqname[bannerSq]; int w = 4 * (int)strlen(nm) + 4, x0 = 32 - w / 2;
      for (int y = 9; y <= 17; y++) for (int x = x0 - 1; x <= x0 + w; x++) if (x >= 0 && x < 64) b[y * 64 + x] = (y == 9 || y == 17 || x == x0 - 1 || x == x0 + w);
      vmx_text(b, x0 + 2, 11, nm);
    }
    if (talkOn) vmx_talk(b, talk, talkPage);
    if (kpOn) vmx_keypad(b, kpBuf, kpSel, kpMsgT ? kpMsg : NULL);
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
    const char *h[] = { "DPAD", "MOVE", "A", "PLACE", "B", "FLOOR", "STICK", "NEXT CELL", "R-STICK", "TURN", "X", "SETTINGS", "Y", "LEVEL" };
    if (mode == M_ROOM) hints(h, 7);
  } else {
    if (!framesTried) load_frames();
    gfx_view_offset(0, 16);                   // (the green gradient behind it is drawn above)
    gfx_scene_begin(); draw_shape(); gfx_scene_end();   // (on the Xbox: on its GPU)
    gfx_view_offset(0, 0);
    tab_row(mode == M_WIRE ? 2 : mode == M_MENU ? 3 : 0);
    snprintf(s, sizeof s, "%s   %d CELLS", L.name, L.nsq); gfx_text5(GW - 12 - gfx_text5_w(1, s), 44, 1, 0x7f8a78, s);
    { int ci = find_sq(co, ce1, ce2);       // the cell under the cursor, by name
      if (ci >= 0 && L.sqname[ci][0]) { snprintf(s, sizeof s, "CELL %d: %s", ci, L.sqname[ci]); gfx_text5(GW - 12 - gfx_text5_w(1, s), 56, 1, 0xf2e01c, s); } }
    if (mode == M_SHAPE) {
      level_controls();
      gfx_dim(8, 44, 100, 100, 0.35f);       // the compass, top left: which way the world's axes point
      Vec3 up; { Vec3 r, u; view_axes(&r, &u); float cp = cosf(pitch), sp = sinf(pitch); up = v3f(u.x * sp + vn.x * cp, u.y * sp + vn.y * cp, u.z * sp + vn.z * cp); }
      icons_compass(8, 44, 100, 100, v3f(eye.x - look.x, eye.y - look.y, eye.z - look.z), up);
    }
    if (vmxWin) {                             // the VMX window: the cell under the cursor, as on the handheld
      static uint8_t b[64 * 64]; memset(b, 0, sizeof b);
      int i = find_sq(co, ce1, ce2);
      if (i >= 0) { int f = sq_face_toward_eye(i); if (sides[i] >> (f & 1) & 1) vmx_room(b, L.wall, L.target, L.box, L.btn, L.kind, L.aux, f, 0, L.player, &S, 0); else vmx_text(b, 12, 28, "NO ROOM"); }
      else vmx_text(b, 4, 28, "EMPTY SPACE");
      draw_vmx(GW / 2 - 96, GH / 2 - 96 + 20, 3, b);   // in the middle of the screen
    }
  }
  if (mode == M_PROPS) {
    int i = propSq, rows[12], nr = prop_rows(i, rows), h = 50 + nr * 26 + 30;
    if (h > 340) h = 340;
    snprintf(s, sizeof s, "CELL %d SETTINGS", i);
    gfx_panel(120, 110, 400, h, s, C_HI);
    for (int k = 0; k < nr; k++) {
      char r[64];
      switch (rows[k]) {
        case P_NAME: snprintf(r, sizeof r, "NAME: %s  (A)", L.sqname[i][0] ? L.sqname[i] : "-"); break;
        case P_MOVE: snprintf(r, sizeof r, "MOVE: %s", FUNCS[func_index(&func[i])]); break;
        case P_SIGNAL: if (trig[i]) snprintf(r, sizeof r, "WORKED BY SIGNAL: %d", trig[i]); else snprintf(r, sizeof r, "WORKED BY SIGNAL: NONE"); break;
        case P_TURNMODE: snprintf(r, sizeof r, "TURNS: %s", func[i].kind == 'R' ? "WHILE ON, BACK WHEN OFF" : "A QUARTER EACH CHANGE"); break;
        case P_MIRX: snprintf(r, sizeof r, "LAYOUT: MIRROR LEFT / RIGHT  (A)"); break;
        case P_MIRY: snprintf(r, sizeof r, "LAYOUT: MIRROR TOP / BOTTOM  (A)"); break;
        case P_TURNLAY: snprintf(r, sizeof r, "LAYOUT: TURN A QUARTER  (A)"); break;
        case P_COPY: snprintf(r, sizeof r, "LAYOUT: COPY  (A)"); break;
        case P_PASTE: snprintf(r, sizeof r, "LAYOUT: PASTE  (A)"); break;
        default: snprintf(r, sizeof r, "DELETE CELL  (A)");
      }
      row(140, 156 + k * 26, 370, k == prow, r);
    }
    const char *hh[] = { "DPAD", "PICK / CHANGE", "B", "DONE" }; hints(hh, 2);
  }
  if (mode == M_TILE) {
    int c = tcell, rows[12], nr = tile_rows(c, rows);
    static FoldGame tg; build(&PL); fold_start(&tg, &PL); char said[FOLD_TEXTLEN * 2];
    if (!fold_describe(&tg, c, said, sizeof said)) snprintf(said, sizeof said, "(NOTHING)");
    if (L.kind[c] == FK_KEYPAD) { char x[48]; snprintf(x, sizeof x, " (THE CODE AT THE START: %d)", fold_code(&tg, c)); strncat(said, x, sizeof said - strlen(said) - 1); }
    int h = 50 + nr * 26 + 70;
    gfx_panel(70, 100, 500, h, "TILE SETTINGS", C_HI);
    for (int k = 0; k < nr; k++) {
      char r[64];
      switch (rows[k]) {
        case T_WORDS: snprintf(r, sizeof r, "WORDS: %.40s  (A)", L.txt[c] ? L.text[L.txt[c] - 1] : "-"); break;
        case T_KSRC: snprintf(r, sizeof r, "CODE: %s", L.kp[L.aux[c] - 1].src == FV_NUMBER ? "A NUMBER" : L.kp[L.aux[c] - 1].src == FV_COUNT ? "A COUNTER" : FOLD_VALUE_NAMES[L.kp[L.aux[c] - 1].src]); break;
        case T_KARG: if (L.kp[L.aux[c] - 1].src == FV_COUNT) snprintf(r, sizeof r, "THE COUNTER SENDING: %d", L.kp[L.aux[c] - 1].arg); else snprintf(r, sizeof r, "NUMBER: %d  (A TYPES IT)", L.kp[L.aux[c] - 1].arg); break;
        case T_KOP: { static const char *O[4] = { "NOTHING", "PLUS", "MINUS", "TIMES" }; snprintf(r, sizeof r, "THEN: %s", O[L.kp[L.aux[c] - 1].op]); break; }
        case T_KN: snprintf(r, sizeof r, "BY: %d  (A TYPES IT)", L.kp[L.aux[c] - 1].n); break;
        case T_KCMP: { static const char *C3[3] = { "EXACTLY THAT", "AT LEAST THAT", "AT MOST THAT" }; snprintf(r, sizeof r, "ENTERED MUST BE: %s", C3[L.kp[L.aux[c] - 1].cmp]); break; }
        case T_WORDS2: snprintf(r, sizeof r, "WORDS WHILE ON: %.33s  (A)", L.aux[c] ? L.text[L.aux[c] - 1] : "-"); break;
        case T_SENDS: { const char *w = L.kind[c] == FK_SIGN ? "READING IT SENDS" : L.kind[c] == FK_KEYPAD ? "THE RIGHT CODE SENDS" : "SENDS SIGNAL";
          if (L.btn[c]) snprintf(r, sizeof r, "%s: %d", w, L.btn[c]); else snprintf(r, sizeof r, "%s: NONE", w); break; }
        case T_WORKED: { const char *w = L.kind[c] == FK_SIGN ? "OTHER WORDS WHILE SIGNAL" : "WORKED BY SIGNAL";
          if (L.drv[c]) snprintf(r, sizeof r, "%s: %d", w, L.drv[c]); else snprintf(r, sizeof r, "%s: NONE", w); break; }
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
    static const uint32_t WCOL[8] = { 0x58d838, 0xf2e01c, 0x4ab8ff, 0xff7a4a, 0xd070ff, 0x30e0c8, 0xff5a8a, 0xb0d030 };
    gfx_rect(0, 36, GW, GH - 66, 0x08100a);                  // a dark blueprint
    for (int gx = -(wScX % 20); gx < GW; gx += 20) gfx_rect(gx, 36, 1, GH - 66, 0x101c12);
    for (int gy = 36 - (wScY % 20); gy < GH - 30; gy += 20) if (gy >= 36) gfx_rect(0, gy, GW, 1, 0x101c12);
    for (int k = 0; k < nwn; k++) for (int i = 0; i < node_nin(&wn[k]); i++) {   // the wires: from each sender to each input
      int s2 = node_in(&wn[k], i); if (!s2) continue;
      int x2, y2; port_xy(&wn[k], i, &x2, &y2);
      for (int j = 0; j < nwn; j++) {
        int o = node_out(&wn[j]), stage = -1;
        if (o == s2) stage = 0;
        else if (wn[j].kind == NK_LOGIC && L.logic[wn[j].ref].op == FL_SHIFT && s2 > o && s2 < o + L.logic[wn[j].ref].count) stage = s2 - o;
        if (stage < 0) continue;
        int x1, y1; port_xy(&wn[j], -1, &x1, &y1); y1 += stage * 4;
        uint32_t c = WCOL[s2 % 8]; int mx = x2 > x1 + 24 ? x2 - 14 - (s2 % 6) * 4 : x1 + 10 + (s2 % 4) * 3;   // turns in the gap before its column, staggered
        if (x2 > x1 + 24) { gfx_thick(x1, y1, mx, y1, 2, c); gfx_thick(mx, y1, mx, y2, 2, c); gfx_thick(mx, y2, x2, y2, 2, c); }
        else { int yb = (y1 > y2 ? y1 : y2) + 30; gfx_thick(x1, y1, mx, y1, 2, c); gfx_thick(mx, y1, mx, yb, 2, c); gfx_thick(mx, yb, x2 - 14, yb, 2, c); gfx_thick(x2 - 14, yb, x2 - 14, y2, 2, c); gfx_thick(x2 - 14, y2, x2, y2, 2, c); }
      }
    }
    if (wHold > 0) {                                          // the wire being carried, to the cursor
      int cx, cy; port_xy(&wn[wcur], wport, &cx, &cy);
      for (int j = 0; j < nwn; j++) if (node_out(&wn[j]) == wHold) { int x1, y1; port_xy(&wn[j], -1, &x1, &y1); gfx_thick(x1, y1, cx, cy, 2, (t >> 2) & 1 ? 0xffffff : WCOL[wHold % 8]); }
    }
    for (int k = 0; k < nwn; k++) {                           // the nodes
      const WNode *n = &wn[k]; int x = n->x - wScX, y = n->y - wScY + 70;
      if (x > GW || y > GH - 30 || x + n->w < 0 || y + n->h < 36) continue;
      char t1[32], t2[32] = "";
      uint32_t edge = n->kind == NK_LOGIC ? 0xf2e01c : n->kind == NK_SINK || n->kind == NK_CELL ? 0x4ab8ff : C_HI;
      if (n->kind == NK_ALL) snprintf(t1, sizeof t1, "ALL TARGETS");
      else if (n->kind == NK_TIMER) { snprintf(t1, sizeof t1, "%s %d", L.timer[n->ref].kind == FT_TICK ? "EVERY" : "AFTER", L.timer[n->ref].n); snprintf(t2, sizeof t2, "MOVES"); }
      else if (n->kind == NK_SRC || n->kind == NK_SINK) { snprintf(t1, sizeof t1, "%s", src_kind(n->ref)); where(n->ref, t2, sizeof t2); }
      else if (n->kind == NK_CELL) { snprintf(t1, sizeof t1, "%.18s", FUNCS[func_index(&func[n->ref])]); where(2 * n->ref * NN, t2, sizeof t2); }
      else { const FoldLogic *q = &L.logic[n->ref]; if (q->op == FL_COUNT || q->op == FL_DELAY || q->op == FL_SHIFT) snprintf(t1, sizeof t1, "%s %d", OPNAMES[q->op], q->count); else snprintf(t1, sizeof t1, "%s", OPNAMES[q->op]); }
      gfx_rect(x, y, n->w, n->h, 0x141e14); gfx_frame(x, y, n->w, n->h, edge);
      gfx_text5(x + 8, y + 5, 1, 0xe8ffd8, t1); if (t2[0]) gfx_text5(x + 8, y + 16, 1, C_DIM, t2);
      if (n->kind == NK_LOGIC) {                              // the gate's symbol
        const FoldLogic *q = &L.logic[n->ref]; int sx = x + n->w - 46, sy = y + n->h / 2 - 9; uint32_t c = 0xf2e01c;
        if (q->op == FL_AND) { gfx_rect(sx, sy, 12, 18, c); gfx_disc(sx + 12, sy + 9, 9, c); }
        else if (q->op == FL_OR || q->op == FL_XOR) { gfx_tri2(sx, sy, sx + 26, sy + 9, sx + 4, sy + 9, c); gfx_tri2(sx, sy + 18, sx + 4, sy + 9, sx + 26, sy + 9, c); gfx_disc(sx + 8, sy + 9, 8, c); gfx_disc(sx + 1, sy + 9, 5, 0x141e14);
          if (q->op == FL_XOR) gfx_thick(sx - 4, sy, sx - 4, sy + 18, 2, c); }
        else if (q->op == FL_NOT) { gfx_tri2(sx, sy, sx, sy + 18, sx + 18, sy + 9, c); gfx_ring(sx + 22, sy + 9, 4, 2, c); }
        else { gfx_frame(sx - 6, sy - 2, 34, 22, c); gfx_text5(sx - 2, sy + 6, 1, c, q->op == FL_LATCH ? "SR" : q->op == FL_COUNT ? "#" : q->op == FL_DELAY ? "..." : ">>"); }
      }
      for (int i = 0; i < node_nin(n); i++) {                 // inputs on the left edge, the output on the right
        int px, py; port_xy(n, i, &px, &py); int on = node_in(n, i) != 0;
        gfx_disc(px, py, 4, on ? WCOL[node_in(n, i) % 8] : 0x141e14); gfx_ring(px, py, 4, 1, 0xe8ffd8);
        if (n->kind == NK_LOGIC && lop_arity(L.logic[n->ref].op) == 2) gfx_text5(px + 6, py - 3, 1, C_DIM, L.logic[n->ref].op == FL_LATCH ? (i ? "R" : "S") : (i ? "C" : "D"));
      }
      if (node_out(n) >= 0) { int px, py; port_xy(n, -1, &px, &py); int o = node_out(n); gfx_disc(px, py, 4, o ? WCOL[o % 8] : 0x141e14); gfx_ring(px, py, 4, 1, 0xe8ffd8); }
    }
    { int cx, cy; port_xy(&wn[wcur], wport, &cx, &cy); for (int w = 0; w < 2; w++) gfx_ring(cx, cy, 8 + w, 1, (t >> 3) & 1 ? C_CURSOR : 0xffffff); }
    for (int k = 0; k < wCols; k++) {                          // the columns' names
      int hx = 18 + k * 170 - wScX; if (hx < -100 || hx > GW) continue;
      gfx_text5(hx, 44, 1, C_DIM, k == 0 ? "SENDS" : k == wCols - 1 ? "WORKS" : "LOGIC");
    }
    if (wAdd >= 0) {                                          // the add list
      gfx_panel(220, 90, 200, 50 + (NOPS + 2) * 22, "ADD", C_HI);
      for (int k = 0; k < NOPS + 2; k++) { char r[32]; if (k < NOPS) snprintf(r, sizeof r, "%s GATE", OPNAMES[k]); else snprintf(r, sizeof r, "TIMER: %s", k == NOPS ? "EVERY N MOVES" : "AFTER N MOVES"); row(240, 132 + k * 22, 170, k == wAdd, r); }
    }
    const char *hh[] = { "DPAD", "PORT", "A", wHold > 0 ? "CONNECT" : "TAKE WIRE", "X", "CUT", "Y", "ADD", "L R", "KIND", "R-STICK", "NUMBER", "B", "BACK" }; hints(hh, 7);
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
