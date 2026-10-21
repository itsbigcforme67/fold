# Writes build/rec.txt: building a level that shows most of FOLD's mechanics, from nothing (the level view and its
# camera, the cell editor and its tile palette, tile settings, the keyboard, the wiring picture), then playing it on the
# VMX to CLEAR. Record and encode (from the repo root, nothing opens on screen):
#   python3 tools/make_demo.py && mkdir -p build/rec build/reclevels && build/fold_headless -l build/reclevels build/rec.txt
#   ffmpeg -framerate 30 -i build/rec/%05d.ppm -vf scale=1280:960:flags=neighbor -c:v libx264 -crf 20 -pix_fmt yuv420p media/fold_builder_demo.mp4
# The level (cell 2, "LAB", as the editor shows it at turn 0; the doorway on its right edge leads to cell 1, "VAULT"):
#   ########     the player starts bottom left on an ice strip; a key, a one-way floor and a lock on the way up;
#   #...L..#     a box goes down onto a plate that is also a target; the keypad wants your move count (the sign says
#   #.bl...#     so, with the live number); plate AND keypad light the light, the light opens the gate, and the other
#   #.=..?.#     box goes through the gate and over the fold onto the target in the VAULT
#   #&..b|.>
#   #...<k.#
#   #p~~~..#
#   ########
L = []; shots = []
SLOW = 1.25
def w(n): L.append("wait %d" % max(1, int(n * SLOW)))
def b(x, n=6): L.append(x); w(n)
def st(dx, dy, n=6): L.append("stick %d %d 1" % (dx, dy)); w(n)
def rs(x, y, n, after=8): L.append("rstick %g %g %d" % (x, y, n)); w(after)
def shot(name): L.append("shot build/shots/c_%s.ppm" % name); shots.append(name)
def cap(t): L.append("caption " + t); w(12 if t else 0)
N = 8
item = 1
pos = [3, 3]
def sel(k):                      # tile picking: the D-pad over the room's right edge onto the palette (4 a row), A takes it
    global item
    if item == k: return
    while pos[0] < N - 1: b("RIGHT", 4); pos[0] += 1
    b("RIGHT", 6)                                    # onto the palette, at the current tile
    while item // 4 > k // 4: b("UP", 4); item -= 4          # up first, down last: the last row is short
    while item % 4 < k % 4: b("RIGHT", 4); item += 1
    while item % 4 > k % 4: b("LEFT", 4); item -= 1
    while item // 4 < k // 4: b("DOWN", 4); item += 4
    b("A", 6)                                        # back to the room where the cursor was
def go(x, y):                    # the cell editor's cursor moves with the D-pad
    while pos[0] < x: b("RIGHT", 4); pos[0] += 1
    while pos[0] > x: b("LEFT", 4); pos[0] -= 1
    while pos[1] < y: b("DOWN", 4); pos[1] += 1
    while pos[1] > y: b("UP", 4); pos[1] -= 1
def put(k, x, y, n=8): sel(k); go(x, y); b("A", n)
# the keyboard: rows of 10 (letters, digits), then SPACE DEL DONE, then (sign words) the live values
KROWS = ["ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ.,!?", "0123456789"]
TOKS = ["MOVES", "PUSHES", "FLIPS", "TURNS", "KEYS", "BOXES", "LIGHTS"]
def kw(y, tokens): return 10 if y < 4 else 3 if y == 4 else 7
def type_text(text, tokens=False):
    x, y, rows = 0, 0, 6 if tokens else 5
    def to(tx, ty):
        nonlocal x, y
        while y != ty:                               # down (wrapping) keeps roughly the same place across rows
            ny = (y + 1) % rows; x = ((2 * x + 1) * kw(ny, tokens)) // (2 * kw(y, tokens)); y = ny; b("DOWN", 3)
        wd = kw(y, tokens)
        r = (tx - x) % wd
        if r <= wd - r:
            for _ in range(r): b("RIGHT", 3)
        else:
            for _ in range(wd - r): b("LEFT", 3)
        x = tx
    i = 0
    while i < len(text):
        if text[i] == "{":
            j = text.index("}", i); to(TOKS.index(text[i + 1:j]), 5); b("A", 4); i = j + 1; continue
        ch = text[i]
        if ch == " ": to(0, 4)
        else: ty = next(r for r in range(4) if ch in KROWS[r]); to(KROWS[ty].index(ch), ty)
        b("A", 4); i += 1
    w(16); b("START", 8)                             # done

L.append("rec build/rec")
w(24)
# ---- the level view: cells, folding, the camera
cap("A: PLACE A CELL"); b("A", 14)
cap("D-PAD: MOVE THE CURSOR  A: ANOTHER"); b("RIGHT", 8); b("A", 14)
cap("LEFT STICK: ROLL OVER THE EDGE"); st(1, 0, 24)
cap("A: A CELL ON THE SIDE"); b("A", 18)
cap("RIGHT STICK: LOOK ROUND"); rs(0.7, 0, 30); rs(-0.7, 0, 40); rs(0, 0.6, 14); rs(0, -0.6, 14)
cap("L / R: ZOOM"); L.append("hold L 22"); w(6); L.append("hold R 22"); w(8)
cap("WHITE / BLACK: A LAYER UP / DOWN"); b("WHITE", 16); b("BLACK", 16)
cap("LEFT STICK CLICK: THE OTHER SIDE"); b("LS", 26); b("LS", 26)
shot("cells")
# ---- the cell editor: the room on the side (cell 2)
cap("Y: THE CELL EDITOR"); b("Y", 22)
cap("RIGHT STICK: TURN THE ROOM"); L.append("rstick -1 0 1"); w(14); L.append("rstick -1 0 1"); w(16); pos[:] = [4, 4]   # opened at turn 2: two turns to turn 0 (the cursor keeps its tile)
cap("D-PAD PAST THE RIGHT EDGE: THE TILES")
put(5, 1, 6)                                         # the player
cap("ICE, A KEY, A LOCK, A ONE-WAY FLOOR")
put(6, 2, 6, 5); put(6, 3, 6, 5); put(6, 4, 6, 6)    # ice
put(12, 5, 5); put(13, 3, 2)                         # a key, a lock
put(7, 4, 5, 4); b("A", 4); b("A", 4); b("A", 8)     # a one-way floor, turned to point left
cap("BOXES, A PLATE THAT IS A TARGET, A GATE")
put(2, 2, 2); put(2, 4, 4)                           # two boxes
put(10, 2, 3, 5); put(3, 2, 3)                       # a plate, and a target on it
put(11, 5, 4); put(1, 7, 4)                          # a gate, and a doorway on the edge
cap("A LIGHT, A KEYPAD, A SIGN")
put(17, 4, 1); put(19, 5, 3); put(18, 1, 4)
shot("lab")
cap("X: TILE SETTINGS - THE SIGN SAYS...")
b("X", 10); b("A", 10)                               # its words, on the keyboard (with live values)
type_text("TYPE YOUR MOVES IN {MOVES}", tokens=True)
w(20); shot("sign"); b("B", 8)
cap("THE KEYPAD: ITS CODE IS YOUR MOVE COUNT")
go(5, 3); b("X", 10); b("RIGHT", 16)                 # code: a number -> moves
shot("keypad"); w(14); b("B", 8)
cap("NAME THE CELL"); go(3, 5); b("X", 10); b("A", 10)   # plain floor: the cell's settings; NAME
type_text("LAB"); b("B", 8)
cap("STICK: JUMP TO THE NEXT CELL"); go(7, 4); st(1, 0, 18); pos[:] = [0, 4]
put(3, 3, 4)                                         # the target, over the fold
b("X", 10); b("DOWN", 6); b("A", 10); b("A", 10); type_text("VAULT"); b("B", 8)   # the target's settings: CELL SETTINGS, NAME
shot("vault")
cap("Y: BACK TO THE LEVEL"); b("Y", 20)
# ---- wiring: plate AND keypad light the light; the light opens the gate
cap("START: MENU  -  WIRING"); b("START", 12); b("DOWN", 6); b("DOWN", 8); b("A", 20)
cap("Y: ADD A GATE (AN AND)"); b("Y", 10); b("A", 16)
cap("A: TAKE A WIRE  A AGAIN: CONNECT IT")
b("DOWN", 5); b("DOWN", 5); b("A", 8); b("RIGHT", 5); b("A", 14)                       # the plate -> the AND
b("LEFT", 5); b("DOWN", 5); b("DOWN", 5); b("DOWN", 5); b("A", 8); b("RIGHT", 5); b("A", 14)   # the keypad -> the AND
cap("THE AND LIGHTS THE LIGHT, THE LIGHT OPENS THE GATE")
b("RIGHT", 6); b("A", 8); b("RIGHT", 6); b("A", 14)                                     # the AND -> the light
b("LEFT", 5); b("LEFT", 5); b("LEFT", 5); b("DOWN", 5); b("A", 8)                      # the light's own output...
b("RIGHT", 5); b("RIGHT", 5); b("DOWN", 5); b("A", 16)                                  # ...-> the gate
w(16); shot("wiring"); b("B", 12)
# ---- playtest on the VMX
cap("START: PLAYTEST ON THE VMX"); b("START", 12); b("A", 24)
cap("A: READ THE SIGN"); b("UP", 10); b("A", 24); w(10); b("START", 8)
cap("ICE"); b("DOWN", 8); b("RIGHT", 16)
cap("THE KEY, THE ONE-WAY FLOOR, THE LOCK")
for m in ["UP", "LEFT", "LEFT", "UP", "UP", "UP", "UP", "LEFT"]: b(m, 7)
cap("THE BOX ONTO THE PLATE"); b("DOWN", 14)
cap("A: THE KEYPAD - 15 MOVES SO FAR")
for m in ["RIGHT", "DOWN", "RIGHT"]: b(m, 7)
b("A", 12)                                           # the keypad: 1, 5, OK
b("A", 6); b("RIGHT", 4); b("DOWN", 4); b("A", 6); b("DOWN", 4); b("DOWN", 4); b("RIGHT", 4); b("A", 30)
cap("THE GATE IS OPEN: OVER THE FOLD")
b("LEFT", 7); b("DOWN", 7)
for _ in range(7): b("RIGHT", 9)
w(60); shot("end")
L.append("rec off")
open("build/rec.txt", "w").write("\n".join(L) + "\n")
print(len(L), "lines;", "shots:", " ".join(shots))
