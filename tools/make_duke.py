# Builds assets/duke.mesh from the Duke controller parts used for the VMX film (XMU GADGET/anim/duke/part_*.stl):
# each part welded, decimated to a few hundred / thousand triangles, edges split at sharp angles, and put into the
# icon frame: x right, y up the controller's face, z out of it (millimetres, around the part's own middle).
# Run headless:  blender -b --factory-startup --python tools/make_duke.py
# File: "DUKE" u32 nparts; per part: char name[12], float centre[3] (where it sits on the pad), u32 nv, u32 nt,
#   nv x (float pos[3], float normal[3]), nt x u16[3].
import bpy, bmesh, struct, os
HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "..", "..", "XMU GADGET", "anim", "duke")
PARTS = [("dpad", 1400), ("stick_l", 1800), ("stick_r", 1800), ("a", 500), ("b", 500), ("x", 500), ("y", 500)]
out = []
for name, target in PARTS:
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    bpy.ops.wm.stl_import(filepath=os.path.join(SRC, "part_%s.stl" % name))
    o = bpy.context.selected_objects[0]; bpy.context.view_layer.objects.active = o; o.data = o.data.copy()
    bm = bmesh.new(); bm.from_mesh(o.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.01); bm.to_mesh(o.data); bm.free()
    m = o.modifiers.new("d", 'DECIMATE'); m.ratio = min(1.0, target / len(o.data.polygons)); m.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier="d")
    m = o.modifiers.new("e", 'EDGE_SPLIT'); m.split_angle = 0.6
    bpy.ops.object.modifier_apply(modifier="e")
    bm = bmesh.new(); bm.from_mesh(o.data); bmesh.ops.triangulate(bm, faces=bm.faces); bm.to_mesh(o.data); bm.free()
    me = o.data
    P = [(-v.co.x, v.co.z, v.co.y) for v in me.vertices]            # STL (x left, y out, z up) -> icon frame (a turn, not a mirror)
    N = [(-v.normal.x, v.normal.z, v.normal.y) for v in me.vertices]
    c = [(min(p[i] for p in P) + max(p[i] for p in P)) / 2 for i in range(3)]
    T = [tuple(f.vertices) for f in me.polygons]
    print("PART", name, len(P), "verts", len(T), "tris", [round(a, 1) for a in c])
    b = struct.pack("<12s3f2I", name.encode(), *c, len(P), len(T))
    b += b"".join(struct.pack("<6f", p[0] - c[0], p[1] - c[1], p[2] - c[2], *n) for p, n in zip(P, N))
    b += b"".join(struct.pack("<3H", *t) for t in T)
    out.append(b)
with open(os.path.join(HERE, "..", "assets", "duke.mesh"), "wb") as f:
    f.write(b"DUKE" + struct.pack("<I", len(out)) + b"".join(out))
