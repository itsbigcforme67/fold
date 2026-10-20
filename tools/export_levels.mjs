// Export the JavaScript version's levels to .fold files (docs/FORMAT.md).
//   node tools/export_levels.mjs [path/to/foldrooms_core.js] [outdir]
import fs from "fs";
import path from "path";
import { pathToFileURL } from "url";
const core = path.resolve(process.argv[2] || "../ui/games/foldrooms_core.js"), out = process.argv[3] || "levels";
const { LEVELS, SHAPES } = await import(pathToFileURL(core).href);
fs.mkdirSync(out, { recursive: true });
const int = v => v.map(x => { const r = Math.round(x); if (Math.abs(r - x) > 1e-6) throw new Error("not on the grid: " + x); return r === 0 ? 0 : r; });
LEVELS.forEach((lv, i) => {
  const squares = SHAPES[lv.shape](), sides = lv.sides || squares.map(q => q.sides);
  const faces = sides.flatMap((sd, k) => [...(sd.includes("f") ? [[k, "f"]] : []), ...(sd.includes("b") ? [[k, "b"]] : [])]);
  const lines = ["fold 1", "name " + lv.shape];
  for (const q of squares) lines.push("square " + [int(q.o), int(q.u), int(q.v)].map(v => v.join(" ")).join("  "));
  lv.rooms.forEach((room, k) => { lines.push("room " + faces[k].join(" ")); lines.push(...room.split("/")); });
  (lv.links || []).forEach((acts, k) => lines.push("link " + (k + 1) + " " + acts.map(a => typeof a === "number" ? "f" + a : a).join(" ")));
  if (lv.sol) lines.push("sol " + lv.sol);
  const name = String(i + 1).padStart(2, "0") + "-" + lv.shape.toLowerCase().replace(/[^a-z0-9]+/g, "-") + ".fold";
  fs.writeFileSync(path.join(out, name), lines.join("\n") + "\n");
});
console.log(LEVELS.length, "levels ->", out);
