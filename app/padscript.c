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
// numbers read by hand: nxdk's C library has no float parsing (sscanf %f / atof)
static int num(const char **r, float *v) {
  const char *c = *r; while (*c == ' ' || *c == '\t') c++;
  int neg = *c == '-'; if (*c == '-' || *c == '+') c++;
  if (!((*c >= '0' && *c <= '9') || *c == '.')) return 0;
  float x = 0, f = 0.1f;
  while (*c >= '0' && *c <= '9') x = x * 10 + (float)(*c++ - '0');
  if (*c == '.') for (c++; *c >= '0' && *c <= '9'; c++, f *= 0.1f) x += (float)(*c - '0') * f;
  *v = neg ? -x : x; *r = c; return 1;
}
static int word(const char **r, char *out, int max) {
  const char *c = *r; while (*c == ' ' || *c == '\t') c++;
  int n = 0; while (*c && *c != ' ' && *c != '\t' && *c != '\r' && n < max - 1) out[n++] = *c++;
  out[n] = 0; *r = c; return n > 0;
}
static int nums(const char *r, int count, float *v) { for (int i = 0; i < count; i++) if (!num(&r, &v[i])) return 0; return 1; }
static Pad held;                          // what "set" leaves held
int padscript_next(Pad *p) {
  while (left == 0) {
    if (haveAfter) { haveAfter = 0; *p = after; return 1; }
    if (!at || !*at) return 0;
    char line[256]; size_t k = strcspn(at, "\n"); if (k > 255) k = 255;
    memcpy(line, at, k); line[k] = 0; at += k; if (*at) at++;
    char w[32] = "", a[64] = ""; int n = 0;
    if (line[0] == '#' || sscanf(line, "%31s", w) < 1) continue;
    const char *r = strstr(line, w) + strlen(w);   // the words after the command (no sscanf: see num())
    cur = held; Pad rel = held;
    float v[5]; const char *r2 = r;
    if (!strcmp(w, "wait") && nums(r, 1, v)) { left = (int)v[0]; continue; }
    if (!strcmp(w, "hold") && word(&r2, a, sizeof a) && nums(r2, 1, v)) { cur.held = bit(a); n = (int)v[0]; }
    else if (!strcmp(w, "stick") && nums(r, 3, v)) { cur.lx = v[0]; cur.ly = v[1]; n = (int)v[2]; }
    else if (!strcmp(w, "rstick") && nums(r, 3, v)) { cur.rx = v[0]; cur.ry = v[1]; n = (int)v[2]; }
    else if (!strcmp(w, "zoom") && nums(r, 2, v)) { cur.held = PAD_BACK; cur.ry = v[0]; n = (int)v[1]; }
    else if (!strcmp(w, "set") && word(&r2, a, sizeof a) && nums(r2, 4, v)) {
      held.held = bit(a); held.lx = v[0]; held.ly = v[1]; held.rx = v[2]; held.ry = v[3]; continue;
    } else if (bit(w)) { cur.held = bit(w); n = 1; }
    else continue;                        // shot, rec, caption, load, save...: not input
    left = n; after = rel; haveAfter = 1;   // then let go for a tick, as the headless runner does
  }
  left--; *p = cur;
  return 1;
}
