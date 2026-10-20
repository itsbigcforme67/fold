// FOLD app: the VMX's 64x64 picture. See vmx.h.
#include "vmx.h"
#include "gfx.h"
#include <string.h>
#include <stdio.h>

#define CELL 7
#define OX 4
#define OY 8
static const char *BOX[7] = { "#######", "#.....#", "#.#.#.#", "#..#..#", "#.#.#.#", "#.....#", "#######" };
static const char *BOXON[7] = { "#######", "#######", "##...##", "##.#.##", "##...##", "#######", "#######" };
static const char *TARGET[7] = { ".......", ".......", "..###..", "..#.#..", "..###..", ".......", "......." };
static const char *MAN[7] = { "..###..", "..###..", "...#...", ".#####.", "...#...", "..#.#..", ".#...#." };
static const char *FLOOR[7] = { ".......", ".......", ".......", "...#...", ".......", ".......", "......." };
static const char *BUTTON[7] = { ".......", ".#####.", ".#...#.", ".#.#.#.", ".#...#.", ".#####.", "......." };

static void px(uint8_t *b, int x, int y, int v) { if (x >= 0 && y >= 0 && x < 64 && y < 64) b[y * 64 + x] = (uint8_t)v; }
static void rect(uint8_t *b, int x0, int y0, int x1, int y1, int v) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) px(b, x, y, v); }
static void sprite(uint8_t *b, int x, int y, const char **s) { for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) if (s[j][i] == '#') px(b, x + i, y + j, 1); }
void vmx_text(uint8_t *b, int x, int y, const char *s) {
  for (; *s; s++, x += 4) { const char *g = gfx_glyph(*s); for (int i = 0; i < 15; i++) if (g[i] == '1') px(b, x + i % 3, y + i / 3, 1); }
}

void vmx_room(uint8_t *b, const uint8_t *wall, const uint8_t *target, const uint8_t *box, const uint8_t *btn, int f, int rot, int player) {
  for (int fy = 0; fy < FOLD_N; fy++) for (int fx = 0; fx < FOLD_N; fx++) {
    int c = f * FOLD_NN + fy * FOLD_N + fx, sx, sy;
    fold_to_screen(fx, fy, rot, &sx, &sy);
    int x = OX + sx * CELL, y = OY + sy * CELL;
    if (wall[c]) {                            // bricks
      for (int j = 0; j < CELL; j++) for (int i = 0; i < CELL; i++)
        if (!(j == 3 || j == 6 || (j < 3 && i == 6) || (j > 3 && j < 6 && i == 2))) px(b, x + i, y + j, 1);
      continue;
    }
    if (!target[c] && !btn[c] && !box[c] && c != player) sprite(b, x, y, FLOOR);
    if (btn[c] && !box[c] && c != player) sprite(b, x, y, BUTTON);
    if (target[c] && !box[c]) sprite(b, x, y, TARGET);
    if (box[c]) sprite(b, x, y, target[c] ? BOXON : BOX);
    if (c == player) sprite(b, x, y, MAN);
  }
}

void vmx_game(uint8_t *b, const FoldGame *g, int level, const char *line2) {
  memset(b, 0, 64 * 64);
  vmx_room(b, g->wall, g->target, g->box, g->btn, g->camf, g->camrot, g->p);
  char s[16]; int left = 0;
  for (int c = 0; c < 2 * g->lv->nsq * FOLD_NN; c++) if (g->box[c] && !g->target[c]) left++;
  snprintf(s, sizeof s, "%02d", level); vmx_text(b, 1, 1, s);
  rect(b, 26, 1, 30, 5, 1); rect(b, 27, 2, 29, 4, 0); px(b, 28, 3, 1);
  snprintf(s, sizeof s, "%d", left); vmx_text(b, 33, 1, s);
  snprintf(s, sizeof s, "%d", g->moves > 9999 ? 9999 : g->moves); vmx_text(b, 64 - 4 * (int)strlen(s), 1, s);
  if (line2) {
    rect(b, 6, 23, 57, 43, 0); rect(b, 6, 23, 57, 23, 1); rect(b, 6, 43, 57, 43, 1); rect(b, 6, 23, 6, 43, 1); rect(b, 57, 23, 57, 43, 1);
    vmx_text(b, 32 - 2 * 6 + 1, 27, "A ROOM"); vmx_text(b, 32 - 2 * (int)strlen(line2) + 1, 35, line2);
  }
}
void vmx_colours(const uint8_t *b, uint32_t *out, uint32_t lit, uint32_t dark) { for (int i = 0; i < 64 * 64; i++) out[i] = b[i] ? lit : dark; }
