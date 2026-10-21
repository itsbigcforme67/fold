// FOLD app without a window: runs a script of pad input and saves screenshots (PPM). For tests; nothing opens on
// screen.   build/fold_headless [-l levels] script.txt
// Script: one command a line. Buttons: A B X Y L R START BACK RS UP DOWN LEFT RIGHT (pressed one frame, then let go a
// frame); "hold <button> <frames>"; "stick <lx> <ly> <frames>"; "rstick <rx> <ry> <frames>"; "wait <frames>";
// "zoom <ry> <frames>" (BACK + right stick); "load <file>"; "save <file>"; "shot <file.ppm>"; "#" comments;
// "set <button|-> lx ly rx ry" (stays held); "rec <dir>" saves every frame as dir/NNNNN.ppm until "rec off";
// "caption <words>" labels the recorded frames ("caption" alone clears it); "bench <n>" times n frames; "state" prints the editor's state line.
#include "app.h"
#include "gfx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static void shot(const char *f) {
  FILE *o = fopen(f, "wb"); if (!o) { printf("can't write %s\n", f); return; }
  fprintf(o, "P6\n%d %d\n255\n", GW, GH);
  for (int i = 0; i < GW * GH; i++) { unsigned c = gfx_px[i]; fputc((c >> 16) & 255, o); fputc((c >> 8) & 255, o); fputc(c & 255, o); }
  fclose(o);
}
static unsigned bit(const char *w) {
  static const char *names[] = { "UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "L", "R", "START", "BACK", "RS" };
  for (int i = 0; i < 13; i++) if (!strcmp(w, names[i])) return 1u << i;
  return 0;
}
static char recDir[200], caption[120]; static int recN;
static void draw_caption(void) {             // a label over the recording, above the hints bar
  if (!caption[0]) return;
  int w = gfx_text5_w(2, caption) + 24, x = (GW - w) / 2;
  gfx_dim(x, GH - 118, w, 30, 0.75f); gfx_rect(x, GH - 118, 3, 30, 0xf2e01c);
  gfx_text5(x + 12, GH - 110, 2, 0xf2e01c, caption);
}
static void run(Pad *p, int frames) {
  for (int i = 0; i < frames; i++) {
    app_frame(p);
    if (recDir[0]) { char f[260]; app_draw(); draw_caption(); snprintf(f, sizeof f, "%s/%05d.ppm", recDir, recN++); shot(f); }
  }
  app_draw();
}
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
    else if (!strcmp(w, "zoom") && sscanf(line, "%*s %f %d", &y, &n) == 2) { p.held = PAD_BACK; p.ry = y; run(&p, n); p.held = 0; p.ry = 0; run(&p, 1); }
    else if (!strcmp(w, "set") && sscanf(line, "%*s %31s %f %f %f %f", a, &p.lx, &p.ly, &p.rx, &p.ry) == 5) p.held = bit(a);   // stays set (- = no button)
    else if (!strcmp(w, "caption")) { char *t = line + 7; while (*t == ' ') t++; snprintf(caption, sizeof caption, "%s", t); caption[strcspn(caption, "\r\n")] = 0; }
    else if (!strcmp(w, "rec") && sscanf(line, "%*s %199s", a) == 1) { if (!strcmp(a, "off")) recDir[0] = 0; else snprintf(recDir, sizeof recDir, "%s", a); }
    else if (!strcmp(w, "bench") && sscanf(line, "%*s %d", &n) == 1) {   // draw n frames (ticking each), print the time a draw takes
      clock_t c0 = clock(); for (int i = 0; i < n; i++) { app_frame(&p); app_draw(); }
      printf("bench: %.2f ms a frame\n", (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC / n);
    }
    else if (!strcmp(w, "state")) { char st[128]; app_state(st, sizeof st); printf("state: %s\n", st); }
    else if (!strcmp(w, "load") && sscanf(line, "%*s %199s", a) == 1) { if (!app_load(a)) printf("line %d: load failed\n", lineno); }
    else if (!strcmp(w, "save") && sscanf(line, "%*s %199s", a) == 1) { if (!app_save(a)) printf("line %d: save failed\n", lineno); }
    else if (!strcmp(w, "shot") && sscanf(line, "%*s %199s", a) == 1) { app_draw(); shot(a); }
    else if (bit(w)) { p.held = bit(w); run(&p, 1); p.held = 0; run(&p, 1); }
    else printf("line %d: ? %s", lineno, line);
  }
  fclose(f);
  return 0;
}
