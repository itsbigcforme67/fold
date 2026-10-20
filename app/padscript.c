// FOLD app: pad scripts played back as input. See padscript.h.
#include "padscript.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *text, *at;                   // the script, and the next line
static Pad cur, after; static int left, haveAfter;   // the pad held for `left` more ticks, then `after` for one
static unsigned bit(const char *w) {
  static const char *names[] = { "UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "L", "R", "START", "BACK", "RS" };
  for (int i = 0; i < 13; i++) if (!strcmp(w, names[i])) return 1u << i;
  return 0;
}
int padscript_open(const char *path) {
  FILE *f = fopen(path, "rb"); if (!f) return 0;
  size_t n = 0, cap = 0; size_t got;            // read in chunks (no fseek / ftell: unreliable on the Xbox's disc)
  text = NULL;
  do {
    if (n + 4096 + 1 > cap) { cap = cap ? cap * 2 : 16384; char *t = realloc(text, cap); if (!t) { free(text); text = NULL; fclose(f); return 0; } text = t; }
    got = fread(text + n, 1, 4096, f); n += got;
  } while (got == 4096);
  text[n] = 0; fclose(f);
  at = text; memset(&cur, 0, sizeof cur); left = 0; haveAfter = 0;
  return 1;
}
static Pad held;                          // what "set" leaves held
int padscript_next(Pad *p) {
  while (left == 0) {
    if (haveAfter) { haveAfter = 0; *p = after; return 1; }
    if (!at || !*at) return 0;
    char line[256]; size_t k = strcspn(at, "\n"); if (k > 255) k = 255;
    memcpy(line, at, k); line[k] = 0; at += k; if (*at) at++;
    char w[32] = "", a[64] = ""; float x = 0, y = 0, z = 0, q = 0; int n = 0;
    if (line[0] == '#' || sscanf(line, "%31s", w) < 1) continue;
    const char *r = strstr(line, w) + strlen(w);   // the words after the command (no %*s: nxdk's sscanf mishandles it)
    cur = held; Pad rel = held;
    if (!strcmp(w, "wait") && sscanf(r, "%d", &n) == 1) { left = n; continue; }
    if (!strcmp(w, "hold") && sscanf(r, "%63s %d", a, &n) == 2) cur.held = bit(a);
    else if (!strcmp(w, "stick") && sscanf(r, "%f %f %d", &x, &y, &n) == 3) { cur.lx = x; cur.ly = y; }
    else if (!strcmp(w, "rstick") && sscanf(r, "%f %f %d", &x, &y, &n) == 3) { cur.rx = x; cur.ry = y; }
    else if (!strcmp(w, "zoom") && sscanf(r, "%f %d", &y, &n) == 2) { cur.held = PAD_BACK; cur.ry = y; }
    else if (!strcmp(w, "set") && sscanf(r, "%63s %f %f %f %f", a, &x, &y, &z, &q) == 5) {
      held.held = bit(a); held.lx = x; held.ly = y; held.rx = z; held.ry = q; continue;
    } else if (bit(w)) { cur.held = bit(w); n = 1; }
    else continue;                        // shot, rec, caption, load, save...: not input
    left = n; after = rel; haveAfter = 1;   // then let go for a tick, as the headless runner does
  }
  left--; *p = cur;
  return 1;
}
