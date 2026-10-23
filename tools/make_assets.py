# FOLD app assets: the brushed-metal panel texture (from the owner's photo): "METL", w, h (u16), then w*h RGB.
# (The VMX window pictures, assets/vmx_small and vmx_large, are the skeleton hologram now, made by
# vmx-world/xbox: make -f Makefile.pc holo.)
#   python3 tools/make_assets.py metal.jpg
import struct, sys, pathlib
from PIL import Image
out = pathlib.Path(__file__).resolve().parents[1] / "assets"
metal = sys.argv[1] if len(sys.argv) > 1 else None
if metal:
    m = Image.open(metal).convert("RGB")
    m = m.resize((m.width // 2, m.height // 2), Image.LANCZOS)          # 270 x 180: tiled across the panels
    (out / "metal.rgb").write_bytes(struct.pack("<4s2H", b"METL", m.width, m.height) + m.tobytes())
    print("metal", m.size)
