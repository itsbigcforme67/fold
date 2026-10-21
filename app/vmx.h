// FOLD app: the VMX's 64x64 1-bit picture, drawn the same way as the JavaScript version (ui/games/foldrooms_core.js):
// a room is 8x8 cells of 7 px at (4, 8), the HUD on top. Used for the VMX window and for the rooms on 3D tiles.
#ifndef VMX_H
#define VMX_H
#include <stdint.h>
#include "../core/fold.h"

#define VMX_LIT 0x7dff3a
#define VMX_DARK 0x060a05

// one side's room (face f, turned by rot) into a 64x64 bit buffer; player = cell of the player or -1. With a
// surface, an edge cell you can walk out of (floor in the next room, right now) gets an arrow pointing out.
// VMX_FLOOR_ONLY leaves walls, boxes and the player out (the 3D view stands those up itself).
enum { VMX_FLOOR_ONLY = 1 };
void vmx_room(uint8_t *b, const uint8_t *wall, const uint8_t *target, const uint8_t *box, const uint8_t *btn,
              const uint8_t *kind, const uint8_t *aux, int f, int rot, int player, const FoldSurface *s, int flags);
// the whole game screen: room, HUD (level, boxes left, moves), and a pop-up ("A ROOM" / line2) if line2
void vmx_game(uint8_t *b, const FoldGame *g, int level, const char *line2);
void vmx_text(uint8_t *b, int x, int y, const char *s);
// what a tile says, in a box over the room: word-wrapped to 14 characters, 7 lines a page. Returns the page count.
int vmx_talk(uint8_t *b, const char *text, int page);
extern const char *VMX_BOX[7], *VMX_BOXON[7], *VMX_MAN[7];   // 7x7 sprites, '#' = lit
void vmx_keypad(uint8_t *b, const char *entered, int sel, const char *msg);   // the keypad over the room
const char **vmx_icon(const char *name);   // a tile's 7x7 sprite by its editor name (for palettes)
// 64x64 bits -> 0xRRGGBB (lit / dark)
void vmx_colours(const uint8_t *b, uint32_t *out, uint32_t lit, uint32_t dark);
#endif
