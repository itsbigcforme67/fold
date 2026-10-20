# Writes build/rec.txt: building a small logic level from nothing (every camera control, every menu, wiring),
# then playing it on the VMX. Record and encode (from the repo root, nothing opens on screen):
#   python3 tools/make_demo.py && mkdir -p build/rec build/reclevels && build/fold_headless -l build/reclevels build/rec.txt
#   ffmpeg -framerate 30 -i build/rec/%05d.ppm -vf scale=1280:960:flags=neighbor -c:v libx264 -crf 20 -pix_fmt yuv420p media/fold_builder_demo.mp4
L = []; shots = []
SLOW = 2
def w(n): L.append("wait %d" % int(n * SLOW))
def b(x, n=8): L.append(x); w(n)
def st(dx, dy, n=7): L.append("stick %d %d 1" % (dx, dy)); w(n)
def rs(x, y, n, after=10): L.append("rstick %g %g %d" % (x, y, int(n * 1.5))); w(after)
def zoom(y, n, after=10): L.append("zoom %g %d" % (y, int(n * 1.5))); w(after)
def shot(name): L.append("shot build/shots/c_%s.ppm" % name); shots.append(name)
item = 1
def cap(t): L.append("caption " + t); w(18 if t else 0)
def sel(k):
    global item
    while item != k:
        fwd = (k - item) % 18
        if fwd <= 9: b("R", 5); item = (item + 1) % 18
        else: b("L", 5); item = (item - 1) % 18
pos = [3, 3]
def go(x, y):
    while pos[0] < x: st(1, 0); pos[0] += 1
    while pos[0] > x: st(-1, 0); pos[0] -= 1
    while pos[1] < y: st(0, 1); pos[1] += 1
    while pos[1] > y: st(0, -1); pos[1] -= 1
def put(k, x, y): sel(k); go(x, y); b("A", 12)

L.append("rec build/rec")
w(30)
# ---- the level view: cells, folding, the camera
cap("A: PLACE A CELL"); b("A", 20)                      # first cell
cap("STICK: MOVE THE CURSOR"); st(1, 0, 10); cap("A: PLACE A CELL"); b("A", 18)        # second, beside it
st(0, -1, 10); st(0, 1, 10)     # the cursor wanders
cap("D-PAD: FOLD THE PLANE"); b("RIGHT", 30)                  # D-pad: fold the working plane down
b("A", 22)                      # third cell, hanging over the edge
shot("cells"); w(45)
cap("RIGHT STICK: ORBIT"); rs(0.7, 0, 30); rs(-0.7, 0, 45); rs(0, -0.6, 15); rs(0, 0.6, 15)   # right stick: orbit
cap("L / R: TURN THE VIEW"); b("L", 25); b("R", 25)          # L / R: quarter turns
cap("BACK + RIGHT STICK: ZOOM"); zoom(-0.7, 25); zoom(0.7, 25)   # BACK + right stick: zoom
shot("camera"); w(45)
cap("Y: CELL SETTINGS"); b("Y", 25)                      # cell settings
b("RIGHT", 18); b("RIGHT", 18); b("DOWN", 15); b("UP", 12); b("LEFT", 14); b("LEFT", 20)
shot("cellset"); w(45)
b("B", 15)
# ---- the cell editor
cap("X: CELL EDITOR   L / R: PICK A TILE")
b("X", 25)
put(5, 2, 5)                    # player
put(2, 3, 3); put(3, 3, 2)      # a box under a target...
cap("Y: TILE SETTINGS"); b("Y", 20); b("RIGHT", 25); w(40); b("B", 15)   # ...that sends signal 1 while a box is on it
put(15, 5, 5)                   # a sensor (on while you stand on it)
put(2, 4, 4)                    # the box to deliver
put(11, 6, 4)                   # a gate (starts shut)
cap("FLOOR ON THE EDGE MAKES A DOORWAY"); put(1, 7, 4)                    # floor on the edge: a doorway into the next cell
put(17, 6, 2)                   # a light
shot("cell2"); w(45)
cap("D-PAD JUMPS TO THE NEXT CELL"); go(7, 4); b("RIGHT", 22)        # D-pad: through the doorway into the next cell
pos[:] = [0, 4]
put(3, 3, 4)                    # the target, over the fold
shot("cell1"); w(45)
cap(""); b("LEFT", 22); pos[:] = [7, 4]  # back
b("X", 25)                      # level view
cap("CLICK RIGHT STICK: VMX"); b("RS", 40); b("RS", 15)        # right stick click: the VMX window
# ---- the menu: every row, then wiring
cap("START: MENU")
b("START", 25)
for _ in range(6): b("DOWN", 12)
b("UP", 10)
for _ in range(4): b("UP", 10)
b("RIGHT", 20); b("RIGHT", 15)  # VMX WINDOW on / off
b("DOWN", 15); shot("menu"); w(45)
cap("WIRING: A LATCH AND AN AND"); b("A", 25)                      # WIRING
b("DOWN", 12); b("DOWN", 12); b("A", 20)   # add a logic piece
b("L", 8); b("L", 12)                       # its kind...
for _ in range(4): b("RIGHT", 10)           # ...AND, OR, XOR, NOT, LATCH
b("R", 8); b("R", 12); b("RIGHT", 10); b("RIGHT", 15)   # set by signal 2 (the sensor): out 4, once is enough
b("DOWN", 12); b("A", 20)                   # another piece: an AND (out 5)
b("RIGHT", 15)                              # the target (1)...
b("R", 10)
for _ in range(4): b("RIGHT", 10)           # ...and the latch (4)
w(25); shot("wiring"); w(45)
b("B", 20)
# ---- back in: the gate and the light follow the AND
b("X", 25)
cap("THE GATE OPENS WHEN THE LIGHT IS ON"); go(6, 4); b("Y", 20)            # the gate's settings
for _ in range(3): b("RIGHT", 10)   # worked by signal 3
w(20); shot("gate"); w(45)
b("B", 15)
cap("THE LIGHT IS LIT BY THE AND"); go(6, 2); b("Y", 20)            # the light's settings
b("DOWN", 12)                   # lit by: the AND (signal 5); the gate opens off the light's own signal 3
for _ in range(5): b("RIGHT", 10)
w(20); shot("light"); w(45)
b("B", 15)
b("X", 25)
cap("PLAYTEST ON THE VMX"); b("START", 20); b("A", 30)      # PLAYTEST
# ---- playing on the VMX
for m in ["UP", "RIGHT", "UP"]: b(m, 12)  # box onto the target
cap("A: TALK TO A TILE"); b("A", 30); w(60); b("START", 15)         # talk to the target
for m in ["RIGHT", "RIGHT", "DOWN"]: b(m, 12)
cap("THE SENSOR SETS THE LATCH"); b("DOWN", 30)                             # the sensor: the latch sets, the gate opens, the light comes on
cap("B: UNDO"); b("B", 20); b("DOWN", 20)                 # undo, and again
cap("THROUGH THE GATE AND OVER THE FOLD")
for m in ["LEFT", "LEFT", "UP"]: b(m, 12)
for _ in range(7): b("RIGHT", 14)   # through the gate, over the fold
w(90); shot("end")
L.append("rec off")
open("build/rec.txt", "w").write("\n".join(L) + "\n")
print(len(L), "lines;", "shots:", " ".join(shots))
