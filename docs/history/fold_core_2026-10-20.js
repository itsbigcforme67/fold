// FOLD for the VMX's 64x64 1-bit screen: Sokoban on the outside of a shape (working name), ours. A level is a net of
// 5x5 faces that folds up into a solid (only the cube so far); walking or pushing off a face's edge carries on over
// the fold onto the next face, so boxes go round corners. The screen shows the face you are on in the middle with a
// strip of each of its four neighbours laid flat round it (the "plus" of the net); the corners of the screen, where
// no face is, hold the level, the moves, the boxes left and a die showing which face you are on.
// The surface is built from the net by folding it in 3D and matching up edges, so any net works, and other shapes
// made of squares (polycubes) can come later by swapping the fold step.
// Controls: D-pad walks / pushes (repeats while held); A undo, hold A restart; hold B + D-pad to look at the next
// faces over (let go of B to come back). After a clear, A goes on to the next level.
// Input: held keys {up, down, left, right, a, b}. Pure logic + drawing into a 64x64 bit buffer, 30 Hz.

export const W = 64, H = 64, N = 5, NN = N * N;
const DX = [0, 1, 0, -1], DY = [-1, 0, 1, 0], KEYS = ["up", "right", "down", "left"];
const CELL = 8, OX = 12, OY = 12, FACE = N * CELL;          // the middle face sits at 12..51
const MOVE_T = 3, SLIDE_T = 12, HOLD_RESTART = 30;
const F = {
  A:"010101111101101",B:"110101110101110",C:"011100100100011",D:"110101101101110",E:"111100110100111",F:"111100110100100",
  G:"011100101101011",H:"101101111101101",I:"111010010010111",K:"101101110101101",L:"100100100100111",M:"101111111101101",
  N:"110101101101101",O:"010101101101010",P:"110101110100100",R:"110101110101101",S:"011100010001110",T:"111010010010010",
  U:"101101101101111",V:"101101101101010",W:"101101111111101",X:"101101010101101",Y:"101101010010010",
  "0":"111101101101111","1":"010110010010111","2":"110001010100111","3":"110001010001110","4":"101101111001001",
  "5":"111100110001110","6":"011100111101111","7":"111001010010010","8":"111101111101111","9":"111101111001110",
  "!":"010010010000010",":":"000010000010000"," ":"000000000000000" };
const SPR = {
  box:    ["........", ".######.", ".##..##.", ".#.##.#.", ".#.##.#.", ".##..##.", ".######.", "........"],
  boxOn:  ["........", ".######.", ".######.", ".##..##.", ".##..##.", ".######.", ".######.", "........"],
  target: ["........", "........", "...##...", "..#..#..", "..#..#..", "...##...", "........", "........"],
  man:    ["...##...", "...##...", "..####..", ".#.##.#.", "...##...", "..#..#..", "..#..#..", "........"],
  floor:  ["........", "........", "........", "...#....", "........", "........", "........", "........"] };
const EYE = ["...#####...", ".##.....##.", "#...###...#", ".##.....##.", "...#####..."];
const PIPS = [[], [[1, 1]], [[0, 0], [2, 2]], [[0, 0], [1, 1], [2, 2]], [[0, 0], [2, 0], [0, 2], [2, 2]],
  [[0, 0], [2, 0], [1, 1], [0, 2], [2, 2]], [[0, 0], [2, 0], [0, 1], [2, 1], [0, 2], [2, 2]]];

// ---- levels ------------------------------------------------------------------------------------------------------
// Nets of 5x5 faces (any layout that folds into the shape). # wall  . floor  x target  b box  B box on a target
// p player  P player on a target; spaces are outside the net. sol = a solution as face-relative directions
// (0 up 1 right 2 down 3 left in the frame of the face the player is on), made by tools/fold_gen.mjs; the demo plays it.
export const LEVELS = [
  { map: [ // 1: pushes 4, moves 8, edge crossings 1
    "     ..#..",
    "     .....",
    "     #.#.#",
    "     .....",
    "     ..#..",
    "#...######..#..##.##",
    ".#p#.#####.....#.x.#",
    "b....######.#.#.....",
    ".#.#.#####.....#...#",
    "#...######..#..##.##",
    "     #...#",
    "     .....",
    "     ..#..",
    "     .....",
    "     #...#"], sol: "23333230" },
  { map: [ // 2: pushes 6, moves 10, edge crossings 2
    "     #####",
    "     #####",
    "     #####",
    "     #####",
    "     #####",
    "###..#####.....#...#",
    "#....#####...#......",
    "#.########...#...#..",
    "..#..#####.###......",
    "..x..#####.....#.bp#",
    "     ##.##",
    "     #...#",
    "     .....",
    "     #...#",
    "     ##.##"], sol: "0322010333" },
  { map: [ // 3: pushes 13, moves 19, edge crossings 3
    "     #...#",
    "     .#.#.",
    "     ..x..",
    "     .#.#.",
    "     #...#",
    "#..p######.....##.##",
    ".....#####.#...#...#",
    "..#..#####.#........",
    ".....#####.###.#...#",
    "#.b.######.....##.##",
    "     ##.##",
    "     #...#",
    "     .....",
    "     #...#",
    "     ##.##"], sol: "2223221012222000002" },
  { map: [ // 4: pushes 8, moves 17, edge crossings 3
    "     #...#",
    "     .....",
    "     ..#.b",
    "     .....",
    "     #...#",
    "##.##.....#.x.##.#.#",
    "##b##.............x.",
    "....p.......#..#.#.#",
    "##.##...............",
    "##.##.....#...##.#.#",
    "     #.#.#",
    "     .....",
    "     #.#.#",
    "     .....",
    "     #.#.#"], sol: "33000210000011221" },
  { map: [ // 5: pushes 14, moves 31, edge crossings 3
    "     ##.##",
    "     #...#",
    "     .....",
    "     #...#",
    "     ##.##",
    "...###...##.b.###.##",
    "..b.#.#.#......#...#",
    "x...........#.......",
    "#.....#.#.p....#...#",
    "##...#...##...###.##",
    "     .##..",
    "     .#...",
    "     ...#.",
    "     ..##.",
    "     ..x.."], sol: "0011003230000222200111111033032" },
  { map: [ // 6: pushes 23, moves 52, edge crossings 5
    "     ##x##",
    "     #...#",
    "     .....",
    "     #...#",
    "     ##.##",
    "#...##...#..#..#####",
    ".#.#...........#####",
    ".....b.#..#.#.######",
    ".#.#...........#####",
    "#...##...#..#..#####",
    "     ..###",
    "     ..x.#",
    "     ###.#",
    "     ..#.b",
    "     ....p"], sol: "3011103333333100332222321000033311122223330000012100" },
  { map: [ // 7: pushes 12, moves 50, edge crossings 3
    "     #.#.#",
    "     ...b.",
    "     #.#.#",
    "     .x...",
    "     #.#.#",
    "#...##...##...######",
    "...........#.#x#####",
    "..#.xb.#......b#####",
    "...........#.#p#####",
    "#...##...##...######",
    "     ##...",
    "     #....",
    "     .....",
    "     ....#",
    "     ...##"], sol: "03333033332301000111111033333112223303011322110000" },
  { map: [ // 8: pushes 20, moves 67, edge crossings 4
    "     #...#",
    "     .#.#.",
    "     .....",
    "     .#x#.",
    "     #...#",
    "#...#............#..",
    ".....##.....##...#..",
    "..#..#..#....#..##.#",
    ".......##..#.....b.#",
    "#.b.#......##....###",
    "     ##.##",
    "     #.x.#",
    "     .....",
    "     #xb.#",
    "     ##p##"], sol: "0100300303333223011112122320112230333030033322332300010333333332300" },
  { map: [ // 9: pushes 37, moves 86, edge crossings 8
    "     ##.##",
    "     ##.##",
    "     pb...",
    "     ##x##",
    "     ##.##",
    "#.#.##.b.###.###.#.#",
    "..........##.##..b..",
    "#.#.#..#.......#.#.#",
    "..........##.##.....",
    "#.#.##..x###.###.#x#",
    "     ...##",
    "     ....#",
    "     .....",
    "     #....",
    "     ##..."], sol: "11111222223230012222000111111111212110030322011223333333331100333223322000303010100030" },
  { map: [ // 10: pushes 25, moves 108, edge crossings 4
    "     ##.##",
    "     ##.##",
    "     ....b",
    "     ##.##",
    "     ##.##",
    "#######.##x..###.#.#",
    "#######x##....#.....",
    "#####..x...b...#.#.#",
    "#######.###....b....",
    "#######.####.x.#.#.#",
    "     ##.##",
    "     ##.##",
    "     .....",
    "     ##b##",
    "     ##p##"], sol: "000211101111001122333303233010323333000001113321211032301031222223300022211100333111222330000022222111003333" },
];

// ---- the surface -------------------------------------------------------------------------------------------------
const add = (a, b) => a.map((v, i) => v + b[i]), neg = a => a.map(v => -v), key = p => p.join(",");
// Fold a net (block positions on a grid) into a unit cube's faces, each with a corner o and axes u (right), v (down)
// and normal n, then match edges: edge e runs corner e -> corner e+1 clockwise (TL TR BR BL), and two faces meet
// where one's edge runs the other's backwards.
export function surface(blocks) {
  const at = new Map(blocks.map(([x, y], i) => [x + "," + y, i])), fr = [];
  fr[0] = { o: [0, 0, 1], u: [1, 0, 0], v: [0, 1, 0], n: [0, 0, 1] };
  const q = [0];
  while (q.length) {
    const i = q.shift(), { o, u, v, n } = fr[i], [bx, by] = blocks[i];
    for (let d = 0; d < 4; d++) {
      const j = at.get((bx + DX[d]) + "," + (by + DY[d]));
      if (j === undefined || fr[j]) continue;
      fr[j] = d === 1 ? { o: add(o, u), u: neg(n), v, n: u } : d === 3 ? { o: add(o, neg(n)), u: n, v, n: neg(u) }
            : d === 2 ? { o: add(o, v), u, v: neg(n), n: v } : { o: add(o, neg(n)), u, v: n, n: neg(v) };
      q.push(j);
    }
  }
  const ends = new Map(), adj = fr.map(() => []);
  fr.forEach((f, i) => {
    const c = [f.o, add(f.o, f.u), add(add(f.o, f.u), f.v), add(f.o, f.v)];
    for (let e = 0; e < 4; e++) ends.set(key(c[e]) + ">" + key(c[(e + 1) & 3]), [i, e]);
  });
  for (const [k, [i, e]] of ends) {
    const [a, b] = k.split(">"), m = ends.get(b + ">" + a);
    if (!m) throw new Error("net does not close up at face " + i + " edge " + e);
    adj[i][e] = { f: m[0], e: m[1] };
  }
  // the die: face 0 is 1, its opposite 6, then 2/5, 3/4 (opposite = normals point opposite ways)
  const pips = new Array(fr.length).fill(0), pairs = [[1, 6], [2, 5], [3, 4]];
  let k = 0;
  fr.forEach((f, i) => {
    if (pips[i] || k > 2) return;
    pips[i] = pairs[k][0];
    const j = fr.findIndex(h => key(h.n) === key(neg(f.n)));
    if (j >= 0) pips[j] = pairs[k][1];
    k++;
  });
  // nb[c*4+d] = c2*4+d2: one step from cell c heading d lands on c2 heading d2
  const nb = new Int32Array(fr.length * NN * 4);
  for (let c = 0; c < fr.length * NN; c++) for (let d = 0; d < 4; d++) {
    const f = c / NN | 0, x = c % N, y = (c / N | 0) % N, nx = x + DX[d], ny = y + DY[d];
    if (nx >= 0 && ny >= 0 && nx < N && ny < N) { nb[c * 4 + d] = (f * NN + ny * N + nx) * 4 + d; continue; }
    const p = [x, y, N - 1 - x, N - 1 - y][d], { f: f2, e: e2 } = adj[f][d], p2 = N - 1 - p;
    const [x2, y2] = [[p2, 0], [N - 1, p2], [N - 1 - p2, N - 1], [0, N - 1 - p2]][e2];
    nb[c * 4 + d] = (f2 * NN + y2 * N + x2) * 4 + ((e2 + 2) & 3);
  }
  return { F: fr.length, adj, nb, pips };
}

export function parseLevel(rows) {
  const blocks = [];
  for (let by = 0; by * N < rows.length; by++)
    for (let bx = 0; bx * N < rows[by * N].length; bx++) { const ch = rows[by * N][bx * N]; if (ch && ch !== " ") blocks.push([bx, by]); }
  const s = surface(blocks), C = s.F * NN;
  const wall = new Uint8Array(C), target = new Uint8Array(C), box = new Uint8Array(C);
  let player = 0;
  blocks.forEach(([bx, by], f) => {
    for (let y = 0; y < N; y++) for (let x = 0; x < N; x++) {
      const ch = rows[by * N + y][bx * N + x], c = f * NN + y * N + x;
      if (ch === "#") wall[c] = 1;
      if ("xBP".includes(ch)) target[c] = 1;
      if ("bB".includes(ch)) box[c] = 1;
      if ("pP".includes(ch)) player = c;
    }
  });
  return { s, wall, target, box, player, blocks };
}

// ---- game --------------------------------------------------------------------------------------------------------
export function newGame(seed = 1, level = 0) {
  const g = { t: 0, prev: {}, hold: [0, 0, 0, 0], aHold: 0, levels: LEVELS };
  loadLevel(g, level);
  return g;
}
export function loadLevel(g, i) {
  g.level = ((i % g.levels.length) + g.levels.length) % g.levels.length;
  g.L = parseLevel(g.levels[g.level].map); g.s = g.L.s;
  reset(g);
}
function reset(g) {
  g.box = g.L.box.slice(); g.p = g.L.player; g.cam = { f: g.p / NN | 0, rot: 0 }; g.look = null;
  g.moves = 0; g.pushes = 0; g.hist = []; g.anim = null; g.slide = null; g.queue = -1; g.won = 0; g.demoI = 0;
}
export const solved = g => { for (let c = 0; c < g.box.length; c++) if (g.box[c] && !g.L.target[c]) return false; return true; };
const boxesLeft = g => { let k = 0; for (let c = 0; c < g.box.length; c++) if (g.box[c] && !g.L.target[c]) k++; return k; };

// the plus-shaped view round face f (shown turned by rot): [{f, rot, x, y}] with the middle face first
function placements(s, view) {
  const out = [{ f: view.f, rot: view.rot, x: OX, y: OY }];
  for (let sd = 0; sd < 4; sd++) {
    const { f, e } = s.adj[view.f][(sd + view.rot) & 3];
    out.push({ f, rot: (e + 2 - sd) & 3, x: OX + FACE * DX[sd], y: OY + FACE * DY[sd] });
  }
  return out;
}
// face cell (fx, fy) -> its cell on screen inside a placement turned by rot
function toScreen(fx, fy, rot) {
  return rot === 0 ? [fx, fy] : rot === 1 ? [fy, N - 1 - fx] : rot === 2 ? [N - 1 - fx, N - 1 - fy] : [N - 1 - fy, fx];
}
function cellXY(s, view, c) {
  const f = c / NN | 0;
  for (const pl of placements(s, view)) if (pl.f === f) {
    const [sx, sy] = toScreen(c % N, (c / N | 0) % N, pl.rot);
    return [pl.x + sx * CELL, pl.y + sy * CELL];
  }
  return null;
}

function tryMove(g, sd) {
  const d = (sd + g.cam.rot) & 3, e = g.s.nb[g.p * 4 + d], c1 = e >> 2, d1 = e & 3;
  if (g.L.wall[c1]) return false;
  let c2 = -1;
  if (g.box[c1]) {
    c2 = g.s.nb[c1 * 4 + d1] >> 2;
    if (g.L.wall[c2] || g.box[c2]) return false;
  }
  g.hist.push({ p: g.p, box: g.box.slice(), cam: { ...g.cam }, moves: g.moves, pushes: g.pushes });
  const from = cellXY(g.s, g.cam, g.p), to = cellXY(g.s, g.cam, c1);
  g.anim = { t: 0, man: from && to ? [from, to] : null, box: null, boxC: c2 };
  if (c2 >= 0) {
    g.box[c1] = 0; g.box[c2] = 1; g.pushes++;
    const b2 = cellXY(g.s, g.cam, c2);
    g.anim.box = to && b2 ? [to, b2] : null;
  }
  g.p = c1; g.moves++;
  const f1 = c1 / NN | 0;
  if (f1 !== g.cam.f) g.anim.then = { sd, to: { f: f1, rot: (d1 - sd) & 3 } };   // over the edge: the view follows
  return true;
}
function startSlide(g, sd, to) { g.slide = { t: 0, sd, from: { ...g.cam }, to }; g.cam = { ...to }; }

export function step(g, inp) {
  g.t++;
  const press = {};
  for (const k of ["a", "b"]) press[k] = inp[k] && !g.prev[k];
  KEYS.forEach((k, sd) => {                               // D-pad edges, repeating while held
    g.hold[sd] = inp[k] ? g.hold[sd] + 1 : 0;
    press[k] = g.hold[sd] === 1 || (g.hold[sd] > 10 && (g.hold[sd] - 10) % 5 === 0);
  });
  g.prev = { ...inp };
  if (g.anim && ++g.anim.t >= MOVE_T) { const th = g.anim.then; g.anim = null; if (th) startSlide(g, th.sd, th.to); }
  if (g.slide && ++g.slide.t >= SLIDE_T) g.slide = null;
  const busy = g.anim || g.slide;
  let sd = KEYS.findIndex(k => press[k]);
  if (g.won) {
    g.won++;
    if (press.a && g.won > 15) loadLevel(g, g.level + 1);
    return;
  }
  if (!busy && solved(g)) { g.won = 1; g.look = null; return; }
  // A: undo; held: restart
  g.aHold = inp.a ? g.aHold + 1 : 0;
  if (g.aHold === HOLD_RESTART) { reset(g); g.restartFlash = 20; return; }
  if (g.restartFlash) g.restartFlash--;
  if (press.a && !busy && g.hist.length) {
    const h = g.hist.pop();
    g.p = h.p; g.box = h.box; g.cam = h.cam; g.moves = h.moves; g.pushes = h.pushes;
    return;
  }
  // B held: look round (the view pans face to face); let go and it comes back to you
  if (inp.b) {
    if (!g.look) g.look = { ...g.cam };
    if (sd >= 0 && !busy) {
      const { f, e } = g.s.adj[g.cam.f][(sd + g.cam.rot) & 3];
      startSlide(g, sd, { f, rot: (e + 2 - sd) & 3 });
    }
    return;
  }
  if (g.look) { g.cam = g.look; g.look = null; g.slide = null; }
  if (sd >= 0 && busy) { g.queue = sd; return; }
  if (sd < 0 && !busy && g.queue >= 0) { sd = g.queue; g.queue = -1; }
  if (sd >= 0 && !busy) tryMove(g, sd);
}

// the demo plays the level's stored solution, then goes on to the next level
export function demo(g) {
  const sol = g.levels[g.level].sol || "";
  if (g.won) return g.won > 45 && !g.prev.a ? { a: true } : {};
  if (g.moves !== g.demoI) { reset(g); return {}; }         // someone played: start the level over
  if (g.anim || g.slide || KEYS.some(k => g.prev[k]) || g.t % 3) return {};
  if (g.demoI >= sol.length) return {};
  const d = +sol[g.demoI], sd = (d - g.cam.rot) & 3;
  g.demoI++;
  return { [KEYS[sd]]: true };
}

// ---- drawing -----------------------------------------------------------------------------------------------------
const px = (b, x, y, v = 1) => { if (x >= 0 && y >= 0 && x < W && y < H) b[y * W + x] = v; };
function rect(b, x0, y0, x1, y1, v) { for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) px(b, x, y, v); }
function sprite(b, x, y, spr, v = 1) { spr.forEach((r, j) => { for (let i = 0; i < r.length; i++) if (r[i] === "#") px(b, x + i, y + j, v); }); }
function text(b, x, y, s) {
  for (const ch of String(s)) { const f = F[ch] || F[" "]; for (let i = 0; i < 15; i++) if (f[i] === "1") px(b, x + i % 3, y + (i / 3 | 0)); x += 4; }
}
function face(b, g, pl, ox, oy, bright, skip) {
  const x0 = pl.x + ox, y0 = pl.y + oy;
  if (x0 > W || y0 > H || x0 + FACE < 0 || y0 + FACE < 0) return;
  for (let fy = 0; fy < N; fy++) for (let fx = 0; fx < N; fx++) {
    const c = pl.f * NN + fy * N + fx, [sx, sy] = toScreen(fx, fy, pl.rot), x = x0 + sx * CELL, y = y0 + sy * CELL;
    if (g.L.wall[c]) {
      for (let j = 0; j < CELL; j++) for (let i = 0; i < CELL; i++) {
        const lit = bright ? !(j === 3 || j === 7 || (j < 3 && i === 3) || (j > 3 && j < 7 && i === 7)) : !((i + j) & 1);
        if (lit) px(b, x + i, y + j);
      }
      continue;
    }
    if (!g.L.target[c] && !g.box[c] && c !== g.p) sprite(b, x, y, SPR.floor);
    if (g.L.target[c] && !g.box[c]) sprite(b, x, y, SPR.target);
    if (g.box[c] && !(skip && c === skip.box)) sprite(b, x, y, g.L.target[c] ? SPR.boxOn : SPR.box);
    if (c === g.p && !(skip && skip.man)) sprite(b, x, y, SPR.man);
  }
}
function view(b, g, v, ox, oy, skip) {
  const pls = placements(g.s, v);
  for (let i = pls.length - 1; i >= 0; i--) face(b, g, pls[i], ox, oy, i === 0, skip);
}
const lerp = (a, z, k) => Math.round(a + (z - a) * k);
// ---- the cube turn between faces ----------------------------------------------------------------------------------
// The two faces (as they look on screen) become textures on a cube seen in perspective, turned a (0..90 degrees) so
// the next face comes round to the front. Worked out for "next face on the right" (turn about the vertical axis);
// the other directions mirror / transpose the screen and the textures. Each screen pixel is traced back onto
// whichever face it hits; faces turned away are dithered darker, and the outline and the crease are drawn so the
// cube reads.
const CH = FACE / 2, CD = 4 * CH;                              // cube half size, camera distance
const BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5].map(v => (v + 0.5) / 16);
function faceTex(g, v) { const t = new Uint8Array(W * H); face(t, g, { f: v.f, rot: v.rot, x: 0, y: 0 }, 0, 0, true, null); return t; }
function cubeTurn(b, A, B, a, sd) {
  const c = Math.cos(a), s = Math.sin(a), S = W - 1, T = FACE - 1;
  const fl = 20 * (CD - CH) / CH;                                          // the front face spans 40 px at a = 0
  const map = [(x, y, m) => [y, m - x], (x, y) => [x, y], (x, y) => [y, x], (x, y, m) => [m - x, y]][sd];
  const visA = CD * c > CH + 1e-6, visB = CD * s > CH + 1e-6;
  const shadeA = 0.3 + 0.7 * c, shadeB = 0.3 + 0.7 * s;
  const id = new Uint8Array(W * H);                                        // which face each pixel shows
  for (let cy = 0; cy < H; cy++) for (let cx = 0; cx < W; cx++) {
    const X = cx - 31.5, Y = cy - 31.5;
    let tex = null, u = 0, v = 0, sh = 1, k = 0;
    if (visA) {                                                            // old face: (u, v, -h) turned by a
      u = (X * (CD - CH * c) + fl * CH * s) / (fl * c + X * s);
      if (Math.abs(u) <= CH) { const z = -u * s - CH * c; v = Y * (z + CD) / fl; if (Math.abs(v) <= CH) { tex = A; sh = shadeA; k = 1; } }
    }
    if (!tex && visB) {                                                    // next face: (h, v, w) turned by a
      u = (fl * CH * c - X * (CD - CH * s)) / (X * c - fl * s);
      if (Math.abs(u) <= CH) { const z = -CH * s + u * c; v = Y * (z + CD) / fl; if (Math.abs(v) <= CH) { tex = B; sh = shadeB; k = 2; } }
    }
    if (!tex) continue;
    const [tx, ty] = map(Math.min(T, Math.max(0, Math.floor(u + CH))), Math.min(T, Math.max(0, Math.floor(v + CH))), T);
    const [ox, oy] = map(cx, cy, S);
    id[oy * W + ox] = k;
    if (tex[ty * W + tx] && BAYER[(oy & 3) * 4 + (ox & 3)] < sh) px(b, ox, oy);
  }
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {               // outline: the silhouette and the crease
    const k = id[y * W + x];
    if (!k) continue;
    const o = (i, j) => (i < 0 || j < 0 || i >= W || j >= H) ? 0 : id[j * W + i];
    if (o(x - 1, y) !== k || o(x + 1, y) !== k || o(x, y - 1) !== k || o(x, y + 1) !== k) px(b, x, y);
  }
}

export function draw(g, b = new Uint8Array(W * H)) {
  b.fill(0);
  let skip = null;
  if (g.slide) {
    const k = (g.slide.t + 1) / SLIDE_T, e = k < 0.5 ? 2 * k * k : 1 - 2 * (1 - k) * (1 - k);
    cubeTurn(b, faceTex(g, g.slide.from), faceTex(g, g.slide.to), e * Math.PI / 2, g.slide.sd);
  } else {
    const a = g.anim, k = a ? (a.t + 1) / (MOVE_T + 1) : 0;
    if (a) skip = { man: !!a.man, box: a.box ? a.boxC : -1 };
    view(b, g, g.cam, 0, 0, skip);
    if (a && a.box) { const [[x0, y0], [x1, y1]] = a.box; rect(b, lerp(x0, x1, k), lerp(y0, y1, k), lerp(x0, x1, k) + 7, lerp(y0, y1, k) + 7, 0); sprite(b, lerp(x0, x1, k), lerp(y0, y1, k), g.L.target[a.boxC] ? SPR.boxOn : SPR.box); }
    if (a && a.man) { const [[x0, y0], [x1, y1]] = a.man; sprite(b, lerp(x0, x1, k), lerp(y0, y1, k), SPR.man); }
  }
  // the corners of the screen: no face there, so the HUD
  for (const [x, y] of [[0, 0], [52, 0], [0, 52], [52, 52]]) rect(b, x, y, x + 11, y + 11, 0);
  if (g.look) sprite(b, 0, 3, EYE); else text(b, 0, 1, String(g.level + 1).padStart(2, "0"));
  const mv = String(Math.min(999, g.moves)); text(b, 64 - mv.length * 4 + 1, 1, mv);
  for (const [x, y, dx, dy] of [[11, 11, -1, -1], [52, 11, 1, -1], [11, 52, -1, 1], [52, 52, 1, 1]])
    if (!g.slide) { px(b, x + dx, y); px(b, x + 2 * dx, y); px(b, x, y + dy); px(b, x, y + 2 * dy); }   // the face's corners
  sprite(b, -1, 52, SPR.box); text(b, 6, 54, String(boxesLeft(g)));
  rect(b, 52, 52, 63, 63, 1); rect(b, 53, 53, 62, 62, 0);    // the die: which face the view is on
  for (const [i, j] of PIPS[g.s.pips[g.cam.f]] || []) rect(b, 54 + i * 3, 54 + j * 3, 55 + i * 3, 55 + j * 3, 1);
  if (g.restartFlash) { rect(b, 12, 26, 51, 36, 0); text(b, 18, 29, "RESTART"); }
  if (g.won) {
    rect(b, 8, 20, 55, 43, 0); rect(b, 8, 20, 55, 20, 1); rect(b, 8, 43, 55, 43, 1);
    const last = g.level === g.levels.length - 1;
    text(b, last ? 12 : 22, 23, last ? "ALL CLEAR!" : "CLEAR!");
    const m = g.moves + " MOVES"; text(b, 32 - m.length * 2, 30, m);
    if ((g.t >> 4) & 1) text(b, 20, 37, "A: NEXT");
  }
  return b;
}
