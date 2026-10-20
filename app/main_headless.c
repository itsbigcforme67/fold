// FOLD app without a window: runs a script of pad input and saves screenshots (PPM). For tests; nothing opens on
// screen.   build/fold_headless [-l levels] script.txt
// Script: one command a line. Buttons: A B X Y L R START BACK UP DOWN LEFT RIGHT (pressed one frame, then let go a
// frame); "hold <button> <frames>"; "stick <lx> <ly> <frames>"; "rstick <rx> <ry> <frames>"; "wait <frames>";
// "load <file>"; "save <file>"; "shot <file.ppm>"; "#" comments.
#include "app.h"
#include "gfx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void shot(const char *f) {
  FILE *o = fopen(f, "wb"); if (!o) { printf("can't write %s\n", f); return; }
  fprintf(o, "P6\n%d %d\n255\n", GW, GH);
  for (int i = 0; i < GW * GH; i++) { unsigned c = gfx_px[i]; fputc((c >> 16) & 255, o); fputc((c >> 8) & 255, o); fputc(c & 255, o); }
  fclose(o);
}
static unsigned bit(const char *w) {
  static const char *names[] = { "UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "L", "R", "START", "BACK" };
  for (int i = 0; i < 12; i++) if (!strcmp(w, names[i])) return 1u << i;
  return 0;
}
static void run(Pad *p, int frames) { for (int i = 0; i < frames; i++) { app_frame(p); } app_draw(); }
int main(int argc, char **argv) {
  const char *dir = "levels", *script = NULL;
  for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "-l") && i + 1 < argc) dir = argv[++i]; else script = argv[i]; }
  if (!script) { printf("usage: fold_headless [-l levels] script.txt\n"); return 1; }
  FILE *f = fopen(script, "r"); if (!f) { printf("can't open %s\n", script); return 1; }
  app_init(dir);
  Pad p; memset(&p, 0, sizeof p);
  char line[256]; int lineno = 0;
  while (fgets(line, sizeof line, f)) {
    lineno++;
    char w[32] = "", a[200] = ""; float x = 0, y = 0; int n = 0;
    if (line[0] == '#' || sscanf(line, "%31s", w) < 1) continue;
    if (!strcmp(w, "wait") && sscanf(line, "%*s %d", &n) == 1) run(&p, n);
    else if (!strcmp(w, "hold") && sscanf(line, "%*s %31s %d", a, &n) == 2) { p.held = bit(a); run(&p, n); p.held = 0; run(&p, 1); }
    else if (!strcmp(w, "stick") && sscanf(line, "%*s %f %f %d", &x, &y, &n) == 3) { p.lx = x; p.ly = y; run(&p, n); p.lx = p.ly = 0; run(&p, 1); }
    else if (!strcmp(w, "rstick") && sscanf(line, "%*s %f %f %d", &x, &y, &n) == 3) { p.rx = x; p.ry = y; run(&p, n); p.rx = p.ry = 0; run(&p, 1); }
    else if (!strcmp(w, "load") && sscanf(line, "%*s %199s", a) == 1) { if (!app_load(a)) printf("line %d: load failed\n", lineno); }
    else if (!strcmp(w, "save") && sscanf(line, "%*s %199s", a) == 1) { if (!app_save(a)) printf("line %d: save failed\n", lineno); }
    else if (!strcmp(w, "shot") && sscanf(line, "%*s %199s", a) == 1) { app_draw(); shot(a); }
    else if (bit(w)) { p.held = bit(w); run(&p, 1); p.held = 0; run(&p, 1); }
    else printf("line %d: ? %s", lineno, line);
  }
  fclose(f);
  return 0;
}
