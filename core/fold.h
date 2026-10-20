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
#define FOLD_MAXSIG 64                    // signals 1..63 (0 = none)
#define FOLD_MAXACT 8                     // cell moves per signal
#define FOLD_MAXLOGIC 32                  // logic pieces
#define FOLD_MAXTEXT 32                   // tiles with their own text
#define FOLD_TEXTLEN 96
#define FOLD_MAXSOL 4096
#define FOLD_MAXUNDO 2048                 // moves that can be undone
#ifndef FOLD_JOURNAL
#define FOLD_JOURNAL 65536                // logged changes behind them (4 bytes each)
#endif

// tile kinds (kind[]): what a floor tile is, beyond plain floor. aux[] holds its value (a one-way's direction in its
// cell's frame, ...). Walls, targets, boxes and buttons have their own arrays.
enum { FK_FLOOR = 0, FK_ICE = 1, FK_ONEWAY = 2, FK_PIT = 3, FK_TELE = 4, FK_PLATE = 5, FK_GATE = 6, FK_KEY = 7, FK_LOCK = 8, FK_GRATE = 9, FK_SENSOR = 10, FK_LIGHT = 11 };
// ONEWAY: aux = the only way in (0 up 1 right 2 down 3 left). TELE: aux = its pair (1-9; the two pads with it).
// GATE: aux 0x80 = open, 0x40 = looks like a brick wall; a closed gate is a wall to anything entering.
// KEY: picked up when you step on it (aux 0x80 = taken). LOCK: a wall, unless you walk in holding a key (it's used, the
// lock is floor). GRATE: you can cross it, boxes can't. SENSOR: floor that is on while you stand on it.
// TELE aux 0x80 = switched off (pair in the low bits). LIGHT: a solid lamp, aux 0x80 = lit; worked by drv[] and sending
// btn[] while lit, so a light is an output and an input at once (a visible step in the logic).
//
// Signals (1..63): btn[c] is the signal a tile SENDS: a button (plain floor) flips its signal each time a box lands on
// it; a PLATE is on while a box is on it; a SENSOR while you stand on it; a target while a box is on it; a KEY once
// it's taken. One level-wide input: all targets filled. Logic pieces combine signals. drv[c] is the signal that WORKS
// a tile: each time it changes a gate opens / shuts, a teleporter switches on / off, a one-way turns round. A signal's
// links work cells: each change flips / shifts / hinges it (back and forth), turns it a quarter ('r'), or turns it while
// on and back while off ('R').
enum { FL_AND, FL_OR, FL_XOR, FL_NOT, FL_LATCH, FL_COUNT };   // latch: on once its input is (until input 2 resets it)
typedef struct { uint8_t op, n, out, count; uint8_t in[4]; } FoldLogic;   // count: on after that many switch-ons

typedef struct { int8_t o[3], u[3], v[3], n[3]; } FoldSquare;   // n = v x u: the front's outward normal
typedef struct { char kind; uint8_t sq; char spec[3]; } FoldAct; // kind f r s h g; spec "u".."i" or "<e><+|->"; g: sq = gate group

typedef struct {
  char name[32];
  int nsq;
  FoldSquare sq[FOLD_MAXSQ];
  uint8_t wall[FOLD_MAXC], target[FOLD_MAXC], box[FOLD_MAXC], btn[FOLD_MAXC];   // btn = button number (from 1)
  uint8_t kind[FOLD_MAXC], aux[FOLD_MAXC], drv[FOLD_MAXC], txt[FOLD_MAXC];   // txt = 1 + its text, 0 = none
  int player;
  int nbtn;                               // signals numbered so far by buttons / plates in the rooms
  int nact[FOLD_MAXSIG]; FoldAct act[FOLD_MAXSIG][FOLD_MAXACT];   // a signal's cell moves (act[s - 1])
  int nlogic; FoldLogic logic[FOLD_MAXLOGIC];
  int allSig;                             // the signal for "every target has a box" (0 = none)
  int ntext; char text[FOLD_MAXTEXT][FOLD_TEXTLEN];
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

// a move, for undo: where you were and the start of its changes in the journal
typedef struct { int16_t p; uint8_t camf, camrot, pushed, facing; uint32_t j; } FoldUndo;

typedef struct {
  const FoldLevel *lv;
  FoldSurface s;
  uint8_t wall[FOLD_MAXC], target[FOLD_MAXC], box[FOLD_MAXC], btn[FOLD_MAXC], kind[FOLD_MAXC], aux[FOLD_MAXC];
  uint8_t drv[FOLD_MAXC], txt[FOLD_MAXC];
  uint8_t flips[FOLD_MAXSQ], turns[FOLD_MAXSQ], moved[FOLD_MAXSQ];
  uint8_t keys[1];                        // keys you hold (an array so the journal can log it)
  uint8_t toggled[FOLD_MAXSIG];           // a button's signal (flipped each press)
  uint8_t sig[FOLD_MAXSIG];               // every signal's value, as its outputs last saw it
  uint8_t lstate[FOLD_MAXLOGIC], lprev[FOLD_MAXLOGIC];   // a latch's / counter's state; a counter's last input
  int p, camf, camrot;                    // you; the room on screen and how it is turned
  int facing;                             // the way you last moved, in your cell's frame (who you talk to)
  int moves, pushes;
  int nundo; FoldUndo undo[FOLD_MAXUNDO];
  uint32_t nj; uint32_t journal[FOLD_JOURNAL];   // every change a move made: array << 24 | index << 8 | old value
} FoldGame;

// what a move did (fold_move's result bits)
enum { FOLD_MOVED = 1, FOLD_PUSHED = 2, FOLD_CROSSED = 4, FOLD_FLIPPED = 8, FOLD_TURNED = 16, FOLD_SHIFTED = 32,
       FOLD_HINGED = 64, FOLD_TELEPORTED = 128, FOLD_GATES = 256 };

int fold_parse(FoldLevel *lv, const char *text, char *err, int errlen);   // 1 = ok
void fold_surface(FoldSurface *s, const FoldSquare *sq, int nsq);
int fold_step(const FoldSurface *s, int cell, int dir, int *newdir);    // next cell (heading newdir) or -1
void fold_start(FoldGame *g, const FoldLevel *lv);
int fold_move(FoldGame *g, int screendir);   // 0 up 1 right 2 down 3 left, as on screen; returns FOLD_* bits
int fold_undo(FoldGame *g);                  // 1 = undone
int fold_solved(const FoldGame *g);
int fold_facing(const FoldGame *g);         // the tile you face (or -1)
int fold_describe(const FoldGame *g, int c, char *out, int len);   // what a tile says when you talk to it (0 = nothing)
void fold_to_screen(int fx, int fy, int rot, int *sx, int *sy);   // room cell -> screen cell, room turned by rot
FoldSquare fold_geo_pose(FoldSquare q, const FoldAct *a);           // where a shift / hinge puts a square
int fold_overlap(const FoldSquare *sq, int nsq);                    // 1 if two squares are in the same place
#endif
