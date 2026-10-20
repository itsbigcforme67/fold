# FOLD app assets: the VMX window frame, cut from the owner's render of the VMX (XMU GADGET/out/web/vmx_front.png),
# at two sizes where a VMX pixel is a whole number of screen pixels (2 and 3), the device's top part (its screen).
# Each file: "VMXF", w, h, screen x, screen y (u16 little-endian), then w*h RGBA.
# Also the brushed-metal panel texture (from the owner's photo): "METL", w, h (u16), then w*h RGB.
#   python3 tools/make_assets.py [render.png] [metal.jpg]
import struct, sys, pathlib
from PIL import Image
src = sys.argv[1] if len(sys.argv) > 1 else str(pathlib.Path(__file__).resolve().parents[3] / "XMU GADGET/out/web/vmx_front.png")
img = Image.open(src).convert("RGBA").crop((40, 110, 1360, 2200))          # as the VMX simulator page crops it
SX, SY, SW = 0.26129, 0.13928, 0.47750                                      # the screen, as fractions of the crop
out = pathlib.Path(__file__).resolve().parents[1] / "assets"
for name, scale, extra, side in (("vmx_small", 2, 16, 26), ("vmx_large", 3, 26, None)):
    w = round(64 * scale / SW); h = round(w * img.height / img.width)
    big = img.resize((w, h), Image.LANCZOS)
    sx, sy = round(SX * w), round(SY * h)
    left = 0 if side is None else max(0, sx - side)               # the small one: just round the screen
    right = w if side is None else min(w, sx + 64 * scale + side)
    big = big.crop((left, 0, right, min(h, sy + 64 * scale + extra)))
    sx -= left
    data = struct.pack("<4s4H", b"VMXF", big.width, big.height, sx, sy) + big.tobytes()
    (out / (name + ".rgba")).write_bytes(data)
    print(name, big.size, "screen at", (sx, sy))

metal = sys.argv[2] if len(sys.argv) > 2 else None
if metal:
    m = Image.open(metal).convert("RGB")
    m = m.resize((m.width // 2, m.height // 2), Image.LANCZOS)          # 270 x 180: tiled across the panels
    (out / "metal.rgb").write_bytes(struct.pack("<4s2H", b"METL", m.width, m.height) + m.tobytes())
    print("metal", m.size)
