// FOLD core: Sokoban on the outside of a shape made of squares on a grid. Plain C99, no platform code, no
// allocation; the same code runs on PC, the original Xbox and the VMX (ESP32-S3). Level files: docs/FORMAT.md.
//
// A square has two sides ("faces"): face 2i is square i's front, 2i+1 its back. Every side is an 8x8 room (or all
// wall). A cell is face * 64 + y * 8 + x. Rooms are joined by turning about each edge to the first square met; a rim
// (an edge with nothing but your own back beyond it) is a wall.
#ifndef FOLD_H
#define FOLD_H
#include <stdint.h>

#define FOLD_N 8                          // a room is 8 x 8 cells
#define FOLD_NN 64
#define FOLD_MAXSQ 64                     // squares in a level
#define FOLD_MAXF (2 * FOLD_MAXSQ)        // faces (sides)
#define FOLD_MAXC (FOLD_MAXF * FOLD_NN)   // cells
#define FOLD_MAXBTN 16
#define FOLD_MAXACT 8                     // room moves per button
#define FOLD_MAXSOL 4096
#define FOLD_MAXUNDO 2048

typedef struct { int8_t o[3], u[3], v[3], n[3]; } FoldSquare;   // n = v x u: the front's outward normal
typedef struct { char kind; uint8_t sq; char spec[3]; } FoldAct; // kind f r s h; spec "u".."i" or "<e><+|->"

typedef struct {
  char name[32];
  int nsq;
  FoldSquare sq[FOLD_MAXSQ];
  uint8_t wall[FOLD_MAXC], target[FOLD_MAXC], box[FOLD_MAXC], btn[FOLD_MAXC];   // btn = button number (from 1)
  int player;
  int nbtn;
  int nact[FOLD_MAXBTN]; FoldAct act[FOLD_MAXBTN][FOLD_MAXACT];
  int geo[FOLD_MAXSQ];                    // index+1 into act of the square's one shift / hinge, 0 = none
  FoldAct geoAct[FOLD_MAXSQ];
  int nsol; char sol[FOLD_MAXSOL];
} FoldLevel;

typedef struct { int16_t f; int8_t e; } FoldJoin;                // f = -1: a rim (wall)
typedef struct {
  int nsq;
  FoldSquare sq[FOLD_MAXSQ];              // where the squares are now
  FoldJoin adj[FOLD_MAXF][4];
} FoldSurface;

typedef struct { int16_t p, from, to; uint8_t camf, camrot, btn; } FoldUndo;   // to = -1: no push

typedef struct {
  const FoldLevel *lv;
  FoldSurface s;
  uint8_t wall[FOLD_MAXC], target[FOLD_MAXC], box[FOLD_MAXC], btn[FOLD_MAXC];
  uint8_t flips[FOLD_MAXSQ], turns[FOLD_MAXSQ], moved[FOLD_MAXSQ];
  int p, camf, camrot;                    // you; the room on screen and how it is turned
  int moves, pushes;
  int nundo; FoldUndo undo[FOLD_MAXUNDO];
} FoldGame;

// what a move did (fold_move's result bits)
enum { FOLD_MOVED = 1, FOLD_PUSHED = 2, FOLD_CROSSED = 4, FOLD_FLIPPED = 8, FOLD_TURNED = 16, FOLD_SHIFTED = 32,
       FOLD_HINGED = 64 };

int fold_parse(FoldLevel *lv, const char *text, char *err, int errlen);   // 1 = ok
void fold_surface(FoldSurface *s, const FoldSquare *sq, int nsq);
int fold_step(const FoldSurface *s, int cell, int dir, int *newdir);    // next cell (heading newdir) or -1
void fold_start(FoldGame *g, const FoldLevel *lv);
int fold_move(FoldGame *g, int screendir);   // 0 up 1 right 2 down 3 left, as on screen; returns FOLD_* bits
int fold_undo(FoldGame *g);                  // 1 = undone
int fold_solved(const FoldGame *g);
void fold_to_screen(int fx, int fy, int rot, int *sx, int *sy);   // room cell -> screen cell, room turned by rot
#endif
