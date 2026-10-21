// FOLD app on the original Xbox (nxdk): the same editor and game as the PC. The software renderer's 640x480 picture
// is copied to the TV's framebuffer each frame; the Duke pad comes through nxdk's SDL2 game controller. Assets and
// levels are read from the XBE's own folder (D:), see xbox/xbox_compat.h. Built by xbox/Makefile.
#include "app.h"
#include "gfx.h"
#include "padscript.h"
#include "../xbox/gpu.h"
#include <hal/video.h>
#include <hal/xbox.h>
#include <windows.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>

#undef fopen
FILE *xbox_fopen(const char *path, const char *mode) {   // "assets/x" -> "D:\assets\x"; '/' -> '\'
  char p[300]; size_t n = 0;
  if (!strchr(path, ':')) { strcpy(p, "D:\\"); n = 3; }
  for (; *path && n < sizeof p - 1; path++) p[n++] = *path == '/' ? '\\' : *path;
  p[n] = 0;
  return fopen(p, mode);
}

// log lines out of COM1, where xemu's debug-kit Super I/O (-device lpc47m157) writes them to a file; a retail Xbox
// has nothing there (the line status reads 0xFF) and gets nothing
static unsigned char inb(unsigned short port) { unsigned char v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port)); return v; }
static void outb(unsigned short port, unsigned char v) { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port)); }
void fold_serial(const char *s);
void fold_serial(const char *s) {
  if (inb(0x3FD) == 0xFF) return;
  for (; *s; s++) { int t = 100000; while (!(inb(0x3FD) & 0x20) && --t) {} outb(0x3F8, (unsigned char)*s); }
}

static float axis(SDL_GameController *c, SDL_GameControllerAxis a, int flip) {
  float v = SDL_GameControllerGetAxis(c, a) / 32767.0f * (flip ? -1 : 1);
  if (v > -0.2f && v < 0.2f) return 0;                 // dead zone
  return v > 1 ? 1 : v < -1 ? -1 : v;
}
static void read_pad(SDL_GameController *c, Pad *p) {
  memset(p, 0, sizeof *p);
  if (!c) return;
  p->lx = axis(c, SDL_CONTROLLER_AXIS_LEFTX, 0); p->ly = axis(c, SDL_CONTROLLER_AXIS_LEFTY, 0);
  p->rx = axis(c, SDL_CONTROLLER_AXIS_RIGHTX, 0); p->ry = axis(c, SDL_CONTROLLER_AXIS_RIGHTY, 0);
  static const struct { SDL_GameControllerButton b; unsigned bit; } M[] = {
    { SDL_CONTROLLER_BUTTON_DPAD_UP, PAD_UP }, { SDL_CONTROLLER_BUTTON_DPAD_DOWN, PAD_DOWN },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT, PAD_LEFT }, { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, PAD_RIGHT },
    { SDL_CONTROLLER_BUTTON_A, PAD_A }, { SDL_CONTROLLER_BUTTON_B, PAD_B }, { SDL_CONTROLLER_BUTTON_X, PAD_X },
    { SDL_CONTROLLER_BUTTON_Y, PAD_Y }, { SDL_CONTROLLER_BUTTON_START, PAD_START }, { SDL_CONTROLLER_BUTTON_BACK, PAD_BACK },
    { SDL_CONTROLLER_BUTTON_RIGHTSTICK, PAD_RS }, { SDL_CONTROLLER_BUTTON_LEFTSTICK, PAD_LS },
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, PAD_WHITE }, { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, PAD_BLACK } };   // white / black
  for (unsigned i = 0; i < sizeof M / sizeof M[0]; i++) if (SDL_GameControllerGetButton(c, M[i].b)) p->held |= M[i].bit;
  // the triggers are L and R (the Duke's white / black come through as the shoulder buttons)
  if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 12000) p->held |= PAD_L;
  if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000) p->held |= PAD_R;
}

int main(void) {
  XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
  SDL_Init(SDL_INIT_GAMECONTROLLER);           // (before pbkit, as nxdk's samples do)
  int gpu = gpu_init();                         // the 3D scene on the GPU; without it, the software picture as before
  app_init("D:\\levels");
  SDL_GameController *pad = NULL;
  int demo = padscript_open("D:\\demo.txt"), trace = 1;   // a pad script on the disc plays first (tests, attract);
                                                          // trace: its pad and the editor state to COM1
  fold_serial(demo ? "FOLD: playing D:\\demo.txt\n" : "FOLD: started\n");
  fold_serial(gpu ? "FOLD: GPU\n" : "FOLD: no GPU, software picture\n");
  Uint32 last = SDL_GetTicks(), fpsT = last; int frames = 0, fps = 0;
  while (!app_quit()) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = SDL_GameControllerOpen(e.cdevice.which);
      else if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad && SDL_GameControllerFromInstanceID(e.cdevice.which) == pad) { SDL_GameControllerClose(pad); pad = NULL; }
    }
    Pad p; read_pad(pad, &p);
    // the app ticks at 30 Hz: run as many ticks as time has passed (at most 4), then draw once
    Uint32 now = SDL_GetTicks(); int ticks = (int)((now - last) / 33);
    if (ticks < 1) { if (gpu) Sleep(1); else XVideoWaitForVBlank(); continue; }   // (pbkit owns the vblank interrupt: not XVideo's wait)
    if (ticks > 4) ticks = 4;
    last += (Uint32)ticks * 33; if (now - last > 200) last = now;
    for (int k = 0; k < ticks; k++) {
      if (demo && !padscript_next(&p)) { demo = 0; fold_serial("FOLD: demo ended\n"); }
      if (demo && trace) {                    // the pad each tick it changes (to compare with the PC)
        static Pad lp; static int tick;
        if (memcmp(&lp, &p, sizeof p)) { char s[96]; snprintf(s, sizeof s, "T %d %x %d %d %d %d\n", tick, p.held, (int)(p.lx * 100), (int)(p.ly * 100), (int)(p.rx * 100), (int)(p.ry * 100)); fold_serial(s); lp = p; }
        static char lastSt[96]; char st[96]; app_state(st, sizeof st);
        if (strcmp(st, lastSt)) { char s2[128]; snprintf(s2, sizeof s2, "S %d %s\n", tick, st); fold_serial(s2); strcpy(lastSt, st); }
        tick++;
      }
      app_frame(&p);
    }
    if (gpu) gpu_begin();
    app_draw();
    frames++;
    if (now - fpsT >= 1000) { char s[48]; fps = frames; frames = 0; fpsT = now; snprintf(s, sizeof s, "FOLD: %d fps\n", fps); fold_serial(s); }
    if (p.held & PAD_BACK && p.held & PAD_START) { char s[16]; snprintf(s, sizeof s, "%d FPS", fps); gfx_text5(GW - 60, GH - 14, 1, 0xffffff, s); }
    if (gpu) gpu_end(gfx_px);                            // the 2D layer over the GPU's scene
    else { unsigned char *fb = XVideoGetFB(); memcpy(fb, gfx_px, sizeof gfx_px); XVideoFlushFB(); }   // 640x480 X8R8G8B8
  }
  SDL_Quit();
  return 0;                                            // back to the dashboard
}
