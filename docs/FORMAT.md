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
(entered only heading that way, in the room's own frame; it turns with its cell and mirrors when the cell flips),
`_` pit (you can't walk in; a box pushed or sliding in fills it and it becomes floor), `1`-`9` teleporter pad
(stepping on one puts you on the other pad with the same digit, same heading; a pushed box goes across too if the
far pad is free), `=` pressure plate (a button that holds only while a box is on it: when the box leaves, its moves
undo; numbered with the buttons), `|` gate (closed) / `:` gate (open): a wall while closed; opened and closed by the
link move `g<k>`, which works every gate of group k (`gate <square> <f|b> <x> <y> <k>`; the editor uses the number
of the button that works it), `k` key (picked up when you step on it), `l` lock (a wall, unless you walk in holding
a key: it's used up and the lock becomes floor; boxes can't open it), `+` grate (you can cross it, boxes can't).

A box or the player starting on any other kind of tile: `box <square> <f|b> <x> <y>` / `player <square> <f|b> <x> <y>`
after the room block. A side with no `room` block is all wall. The outer ring of a room is wall except for doorways.

A sign: `&` in a room (solid: you read it from beside it with A). Its words: `text <square> <f|b> <x> <y> <words>`;
the words it shows while the signal driving it is on: `text2 ...` (and `drive ... <signal>`). Reading it the first time
turns on the signal it sends (`signal ...`); that can be undone and doesn't count as a move. Words may hold live
values, filled in when read: `{MOVES}` `{PUSHES}` `{FLIPS}` `{TURNS}` `{KEYS}` `{BOXES}` (boxes on targets) `{LIGHTS}`
(lights on) and `{COUNT <signal>}` (how far the counter sending that signal has counted).

A cell's name: `cellname <square> <text>` (up to 15 characters; shown in the editor, and on the VMX as you walk in).

A level with no player loads (an unfinished one, saved from the editor); playing it needs one.

Link entries, applied in order: `f<i>` flip square i (each side's contents move to the same spot on the other
side); `r<i>` turn square i a quarter clockwise as seen from the front; `s<i>:<d>` shift square i one unit, d = u d l
r (in its own frame) or o i (out / in along its normal); `h<i>:<e><+|->` hinge square i 90 degrees about its edge e
(0 top, 1 right, 2 bottom, 3 left) towards its front (+) or back (-); `g<k>` open / close the gates of group k.
Shifts and hinges toggle: the next press puts
the square back. A square has at most one shift or hinge. No move may leave two squares in the same place.

## Signals and logic

Inputs send signals (1-63); logic combines them; outputs follow them. Buttons and plates number their signals in room
order, and these lines set or add the rest:

```
signal <sq> <f|b> <x> <y> <k>     the tile sends signal k: a light `@` / `$` drawn lit (while it's lit), a button (flips each time a box lands on it), plate (on while
                                  a box is on it), sensor `*` (on while you stand on it), target (on while a box is on
                                  it), key (on once taken)
drive <sq> <f|b> <x> <y> <k>      signal k works the tile: a gate (`|` `:`, or `%` `;` that look like brick wall) opens /
                                  shuts, a teleporter switches on / off, a one-way turns round, a light lights. Its state is as drawn,
                                  switched while k is on.
input <k> alltargets              k is on while every target has a box
input <k> tick <n>                k is on for one move every n moves (timers, blinking)
input <k> moves <n>               k is on once you have made n moves (step limits)
logic <out> and|or|xor <in...>    up to 4 inputs; not <in>; latch <set> [<reset>]; count <n> <in> (on after n switch-ons);
                                  delay <n> <in> (its input as it was n moves ago, n up to 8)
link <k> <moves...>               k's cell moves: each change flips / shifts / hinges (back and forth) or turns a quarter
                                  (r); R turns while k is on and back while it's off
text <sq> <f|b> <x> <y> <words>   what the tile says when you talk to it (otherwise it's worked out from the wiring)
```

A light is a solid lamp that is both an output (worked by its `drive` signal) and an input (sending its `signal` while
lit): a step in the logic the player can see and talk to. Logic pieces chain too: a piece's output can be another
piece's input (`logic 3 and 1 2`, `logic 5 or 3 4`).
