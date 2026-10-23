// FOLD app: the level editor, playtesting and the VMX window, on top of the core. Platform layers (main_sdl.c,
// main_headless.c, later the Xbox) feed it a pad each frame and show gfx_px.
#ifndef APP_H
#define APP_H
#include <stdint.h>

enum { PAD_UP = 1, PAD_DOWN = 2, PAD_LEFT = 4, PAD_RIGHT = 8, PAD_A = 16, PAD_B = 32, PAD_X = 64, PAD_Y = 128,
       PAD_L = 256, PAD_R = 512, PAD_START = 1024, PAD_BACK = 2048, PAD_RS = 4096, PAD_LS = 8192,   // RS / LS: right / left stick click
       PAD_BLACK = 16384, PAD_WHITE = 32768 };                                     // the Duke's Black / White: drop / raise the cursor a layer
typedef struct { float lx, ly, rx, ry; unsigned held; } Pad;   // sticks -1..1 (y down = +1)

void app_init(const char *levelsDir);   // where levels live (levels/ in the repo); starts on a new, empty level
int app_load(const char *path);         // 1 = ok
int app_save(const char *path);
void app_frame(const Pad *pad);         // one 30 Hz tick
void app_draw(void);                    // into gfx_px
int app_quit(void);                     // the menu's QUIT was chosen
void app_usb_init(void);                // Xbox: before SDL starts USB (an app adds its USB drivers here)
int app_state(char *b, int n);          // the editor's state as a line of text (for comparing runs)
#endif
