// Plays every level's solution through the JavaScript core and prints the same trace as `build/levels -t`, for the
// parity test (make parity).   node tools/trace_js.mjs [path/to/foldrooms_core.js]
import fs from "fs";
import path from "path";
import { pathToFileURL } from "url";
const core = path.resolve(process.argv[2] || "../ui/games/foldrooms_core.js");
const m = await import(pathToFileURL(core).href);
const files = fs.readdirSync("levels").filter(f => f.endsWith(".fold")).sort();
const sum = a => { let s = 0; for (let c = 0; c < a.length; c++) if (a[c]) s = (s + (c + 1) * a[c]) >>> 0; return s; };
m.LEVELS.forEach((lv, i) => {
  console.log("# levels/" + files[i]);
  const g = m.newGame(1, i);
  let moves = 0;
  for (let f = 0; f < 100000 && !g.won; f++) {
    m.step(g, m.demo(g));
    if (g.moves !== moves) {
      moves = g.moves;
      const C = g.s.F * m.NN, boxes = []; for (let c = 0; c < C; c++) if (g.box[c]) boxes.push(c + ",");
      let sq = ""; for (let k = 0; k < g.s.F >> 1; k++) sq += "" + g.flips[k] + g.turns[k] + g.moved[k];
      console.log(`${g.p} ${g.cam.f} ${g.cam.rot}|${boxes.join("")}|${sum(g.wall)} ${sum(g.target)} ${sum(g.btn)}|${sq}`);
    }
  }
});
