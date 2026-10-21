// FOLD app: the VMX's 64x64 picture. See vmx.h.
#include "vmx.h"
#include "gfx.h"
#include <string.h>
#include <stdio.h>

#define CELL 7
#define OX 4
#define OY 8
const char *VMX_BOX[7] = { "#######", "#.....#", "#.#.#.#", "#..#..#", "#.#.#.#", "#.....#", "#######" };
const char *VMX_BOXON[7] = { "#######", "#######", "##...##", "##.#.##", "##...##", "#######", "#######" };
static const char *TARGET[7] = { ".......", ".......", "..###..", "..#.#..", "..###..", ".......", "......." };
const char *VMX_MAN[7] = { "..###..", "..###..", "...#...", ".#####.", "...#...", "..#.#..", ".#...#." };
static const char *FLOOR[7] = { ".......", ".......", ".......", "...#...", ".......", ".......", "......." };
static const char *ARROW[7] = { ".......", "...#...", "..###..", ".#.#.#.", "...#...", ".......", "......." };   // up
static const char *ICE[7] = { ".......", ".#.....", "..#....", ".......", "....#..", ".....#.", "......." };
static const char *ONEWAY[7] = { "...#...", "..###..", ".#####.", "#######", "..###..", "..###..", "......." };   // up
static const char *PIT[7] = { "#######", "#.#.#.#", "##.#.##", "#.#.#.#", "##.#.##", "#.#.#.#", "#######" };
static const char *TELE[7] = { ".#####.", "#.....#", "#.###.#", "#.#.#.#", "#.###.#", "#.....#", ".#####." };
static const char *PLATE[7] = { ".......", ".#####.", ".#####.", ".##.##.", ".#####.", ".#####.", "......." };
static const char *GATE_SHUT[7] = { "#######", "#.#.#.#", "#.#.#.#", "#.#.#.#", "#.#.#.#", "#.#.#.#", "#######" };
static const char *GATE_OPEN[7] = { "#.....#", ".......", ".......", ".......", ".......", ".......", "#.....#" };
static const char *KEY[7] = { ".......", ".###...", "##.####", ".###.##", ".......", ".......", "......." };
static const char *LOCK[7] = { "#######", "#######", "###.###", "##...##", "###.###", "###.###", "#######" };
static const char *GRATE[7] = { "#.#.#.#", ".......", "#.#.#.#", ".......", "#.#.#.#", ".......", "#.#.#.#" };
static const char *LIGHT_OFF[7] = { "..###..", ".#...#.", ".#...#.", "..#.#..", "..###..", "..###..", "......." };
static const char *LIGHT_ON[7] = { "#.###.#", ".#####.", ".#####.", "#.###.#", "..###..", "..###..", "......." };
static const char *SENSOR[7] = { ".......", "...#...", "..#.#..", ".#.#.#.", "..#.#..", "...#...", "......." };
static const char *SIGN[7] = { "#######", "#.....#", "#.###.#", "#.....#", "#######", "...#...", "...#..." };
static const char *KEYPAD[7] = { "#######", "#.....#", "#.#.#.#", "#.....#", "#.#.#.#", "#.....#", "#######" };
static const char *BRICK[7] = { "######.", "######.", "######.", ".......", "##.####", "##.####", "......." };
static const char *BUTTON[7] = { ".......", ".#####.", ".#...#.", ".#.#.#.", ".#...#.", ".#####.", "......." };

static void px(uint8_t *b, int x, int y, int v) { if (x >= 0 && y >= 0 && x < 64 && y < 64) b[y * 64 + x] = (uint8_t)v; }
static void rect(uint8_t *b, int x0, int y0, int x1, int y1, int v) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) px(b, x, y, v); }
static void sprite(uint8_t *b, int x, int y, const char **s) { for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) if (s[j][i] == '#') px(b, x + i, y + j, 1); }
void vmx_text(uint8_t *b, int x, int y, const char *s) {
  for (; *s; s++, x += 4) { const char *g = gfx_glyph(*s); for (int i = 0; i < 15; i++) if (g[i] == '1') px(b, x + i % 3, y + i / 3, 1); }
}

static void arrow_spr(uint8_t *b, int x, int y, int sd, const char **spr) {   // an arrow sprite turned to point sd
  for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) if (spr[j][i] == '#') {
    int ax = sd == 0 ? i : sd == 1 ? 6 - j : sd == 2 ? 6 - i : j, ay = sd == 0 ? j : sd == 1 ? i : sd == 2 ? 6 - j : 6 - i;
    px(b, x + ax, y + ay, 1);
  }
}
static void arrow(uint8_t *b, int x, int y, int sd) { arrow_spr(b, x, y, sd, ARROW); }
void vmx_room(uint8_t *b, const uint8_t *wall, const uint8_t *target, const uint8_t *box, const uint8_t *btn,
              const uint8_t *kind, const uint8_t *aux, int f, int rot, int player, const FoldSurface *s, int flags) {
  if (flags & VMX_FLOOR_ONLY) player = -1;
  for (int fy = 0; fy < FOLD_N; fy++) for (int fx = 0; fx < FOLD_N; fx++) {
    int c = f * FOLD_NN + fy * FOLD_N + fx, sx, sy;
    fold_to_screen(fx, fy, rot, &sx, &sy);
    int x = OX + sx * CELL, y = OY + sy * CELL;
    if (wall[c]) {                            // bricks
      if (flags & VMX_FLOOR_ONLY) continue;
      for (int j = 0; j < CELL; j++) for (int i = 0; i < CELL; i++)
        if (!(j == 3 || j == 6 || (j < 3 && i == 6) || (j > 3 && j < 6 && i == 2))) px(b, x + i, y + j, 1);
      continue;
    }
    int hasBox = box[c] && !(flags & VMX_FLOOR_ONLY), out = fy == 0 ? 0 : fx == FOLD_N - 1 ? 1 : fy == FOLD_N - 1 ? 2 : fx == 0 ? 3 : -1, nd;
    int e = s && out >= 0 ? fold_step(s, c, out, &nd) : -1;
    if (!target[c] && (!btn[c] || kind[c] == FK_LIGHT) && !hasBox && c != player) {
      if (kind[c] == FK_ONEWAY) arrow_spr(b, x, y, (aux[c] - rot) & 3, ONEWAY);
      else if (kind[c] == FK_PIT) sprite(b, x, y, PIT);
      else if (kind[c] == FK_TELE) sprite(b, x, y, TELE);
      else if (kind[c] == FK_GATE) sprite(b, x, y, aux[c] & 0x80 ? GATE_OPEN : GATE_SHUT);
      else if (kind[c] == FK_KEY) sprite(b, x, y, KEY);
      else if (kind[c] == FK_LOCK) sprite(b, x, y, LOCK);
      else if (kind[c] == FK_GRATE) sprite(b, x, y, GRATE);
      else if (kind[c] == FK_LIGHT) sprite(b, x, y, aux[c] & 0x80 ? LIGHT_ON : LIGHT_OFF);
      else if (kind[c] == FK_SENSOR) sprite(b, x, y, SENSOR);
      else if (kind[c] == FK_SIGN) sprite(b, x, y, SIGN);
      else if (kind[c] == FK_KEYPAD) sprite(b, x, y, KEYPAD);
      else if (e >= 0 && !wall[e]) arrow(b, x, y, (out - rot) & 3);
      else sprite(b, x, y, kind[c] == FK_ICE ? ICE : FLOOR);
    }
    if (btn[c] && !hasBox && c != player) sprite(b, x, y, kind[c] == FK_PLATE ? PLATE : BUTTON);
    if (target[c] && !hasBox) sprite(b, x, y, TARGET);
    if (hasBox) sprite(b, x, y, target[c] ? VMX_BOXON : VMX_BOX);
    if (c == player) sprite(b, x, y, VMX_MAN);
  }
}

void vmx_game(uint8_t *b, const FoldGame *g, int level, const char *line2) {
  memset(b, 0, 64 * 64);
  vmx_room(b, g->wall, g->target, g->box, g->btn, g->kind, g->aux, g->camf, g->camrot, g->p, &g->s, 0);
  char s[16]; int left = 0;
  for (int c = 0; c < 2 * g->lv->nsq * FOLD_NN; c++) if (g->box[c] && !g->target[c]) left++;
  snprintf(s, sizeof s, "%02d", level); vmx_text(b, 1, 1, s);
  rect(b, 26, 1, 30, 5, 1); rect(b, 27, 2, 29, 4, 0); px(b, 28, 3, 1);
  snprintf(s, sizeof s, "%d", left); vmx_text(b, 33, 1, s);
  if (g->keys[0]) {                           // keys you hold: a little key and how many
    rect(b, 40, 2, 42, 4, 1); px(b, 41, 3, 0); rect(b, 43, 3, 45, 3, 1); px(b, 45, 4, 1);
    snprintf(s, sizeof s, "%d", g->keys[0] > 9 ? 9 : g->keys[0]); vmx_text(b, 47, 1, s);
  }
  snprintf(s, sizeof s, "%d", g->moves > 9999 ? 9999 : g->moves); vmx_text(b, 64 - 4 * (int)strlen(s), 1, s);
  if (line2) {
    rect(b, 6, 23, 57, 43, 0); rect(b, 6, 23, 57, 23, 1); rect(b, 6, 43, 57, 43, 1); rect(b, 6, 23, 6, 43, 1); rect(b, 57, 23, 57, 43, 1);
    vmx_text(b, 32 - 2 * 6 + 1, 27, "A ROOM"); vmx_text(b, 32 - 2 * (int)strlen(line2) + 1, 35, line2);
  }
}
static int wrap(const char *t, char lines[][15], int max) {   // words into lines of up to 14 characters
  int n = 0, len = 0; lines[0][0] = 0;
  while (*t && n < max) {
    while (*t == ' ') t++;
    int w = 0; while (t[w] && t[w] != ' ') w++;
    if (!w) break;
    if (len && len + 1 + w > 14) { n++; len = 0; if (n == max) break; lines[n][0] = 0; }
    if (len) lines[n][len++] = ' ';
    for (int i = 0; i < w && len < 14; i++) lines[n][len++] = t[i];
    lines[n][len] = 0; t += w;
  }
  return len ? n + 1 : n;
}
int vmx_talk(uint8_t *b, const char *text, int page) {
  static char lines[40][15];
  int n = wrap(text, lines, 40), pages = (n + 6) / 7;
  rect(b, 1, 9, 62, 62, 0); rect(b, 1, 9, 62, 9, 1); rect(b, 1, 62, 62, 62, 1); rect(b, 1, 9, 1, 62, 1); rect(b, 62, 9, 62, 62, 1);
  for (int k = 0; k < 7 && page * 7 + k < n; k++) vmx_text(b, 4, 12 + k * 7, lines[page * 7 + k]);
  if (page + 1 < pages) { px(b, 58, 59, 1); px(b, 59, 59, 1); px(b, 60, 59, 1); px(b, 59, 60, 1); }   // more: a little arrow
  return pages;
}
// a keypad over the room: the number so far, then 1-9, < (delete), 0, OK; sel = the key picked (0..11), msg = a word
void vmx_keypad(uint8_t *b, const char *entered, int sel, const char *msg) {
  rect(b, 8, 9, 55, 62, 0); rect(b, 8, 9, 55, 9, 1); rect(b, 8, 62, 55, 62, 1); rect(b, 8, 9, 8, 62, 1); rect(b, 55, 9, 55, 62, 1);
  rect(b, 12, 12, 51, 20, 1); rect(b, 13, 13, 50, 19, 0);
  vmx_text(b, 50 - 4 * (int)strlen(msg ? msg : entered), 14, msg ? msg : entered);
  static const char *K[12] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "<", "0", "OK" };
  for (int k = 0; k < 12; k++) {
    int x = 13 + (k % 3) * 13, y = 24 + (k / 3) * 9;
    vmx_text(b, x + 6 - 2 * (int)strlen(K[k]), y + 2, K[k]);
    if (k == sel) { rect(b, x, y, x + 11, y, 1); rect(b, x, y + 8, x + 11, y + 8, 1); rect(b, x, y, x, y + 8, 1); rect(b, x + 11, y, x + 11, y + 8, 1); }
  }
}
const char **vmx_icon(const char *n) {
  static const struct { const char *name; const char **spr; } T[] = {
    { "WALL", BRICK }, { "FLOOR", FLOOR }, { "BOX", VMX_BOX }, { "TARGET", TARGET }, { "BUTTON", BUTTON }, { "PLAYER", VMX_MAN },
    { "ICE", ICE }, { "ONE-WAY", ONEWAY }, { "PIT", PIT }, { "TELEPORTER", TELE }, { "PLATE", PLATE }, { "GATE", GATE_SHUT },
    { "KEY", KEY }, { "LOCK", LOCK }, { "GRATE", GRATE }, { "SENSOR", SENSOR }, { "WALL GATE", BRICK }, { "LIGHT", LIGHT_ON }, { "SIGN", SIGN }, { "KEYPAD", KEYPAD } };
  for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) if (!strcmp(T[i].name, n)) return T[i].spr;
  return FLOOR;
}
void vmx_colours(const uint8_t *b, uint32_t *out, uint32_t lit, uint32_t dark) { for (int i = 0; i < 64 * 64; i++) out[i] = b[i] ? lit : dark; }
