# Level file format (`.fold`, version 1)

Plain text, one item per line; `#` starts a comment. Everything is on an integer grid. (In player-facing words a
`square` is a *cell* and each of its 8x8 grid places holds a *tile*.)

```
fold 1
name DRAWBRIDGE                  shape name, shown when the level is cleared
square 0 0 0  1 0 0  0 1 0       corner o, axis u (room's right), axis v (room's down): one line per square
...                              (square i = the i-th square line, from 0; u and v are unit grid vectors)
room 12 f                        square 12, front side (f) or back (b); then 8 rows of 8 cells
########
#..p...#
...
link 1 h12:3+                    what button 1 does each time a box lands on it (buttons are numbered from 1,
                                 in the order rooms are listed, row by row)
sol 0012303...                   optional: a solution, room-relative directions (0 up 1 right 2 down 3 left)
```

The front of square i faces n = v x u (outwards: seen from the front, u points right and v down). The back is the
mirror image: corner o + u, axes -u, v.

Tiles: `#` wall, `.` floor, `x` target, `b` box, `B` box on a target, `p` player, `P` player on a target, `o` button,
`O` box on a button, `~` ice (you and pushed boxes slide across it until something stops you or the ice ends), `^ > v <` one-way floor
(entered only heading that way, in the room's own frame; it turns with its cell and mirrors when the cell flips).

A box or the player starting on any other kind of tile: `box <square> <f|b> <x> <y>` / `player <square> <f|b> <x> <y>`
after the room block. A side with no `room` block is all wall. The outer ring of a room is wall except for doorways.

Link entries, applied in order: `f<i>` flip square i (each side's contents move to the same spot on the other
side); `r<i>` turn square i a quarter clockwise as seen from the front; `s<i>:<d>` shift square i one unit, d = u d l
r (in its own frame) or o i (out / in along its normal); `h<i>:<e><+|->` hinge square i 90 degrees about its edge e
(0 top, 1 right, 2 bottom, 3 left) towards its front (+) or back (-). Shifts and hinges toggle: the next press puts
the square back. A square has at most one shift or hinge. No move may leave two squares in the same place.
