#!/usr/bin/env python3
"""Play feature graphic: 1024x500, no alpha. Calm cream panel with the
wordmark on the left, a real marbling print bleeding in from the right."""
from PIL import Image, ImageDraw, ImageFont
import sys

SRC   = sys.argv[1] if len(sys.argv) > 1 else 'hero-sumi.png'
OUT   = sys.argv[2] if len(sys.argv) > 2 else 'play-feature-1024x500.png'
W, H  = 1024, 500
SPLIT = 0.46                     # cream panel width

art = Image.open(SRC).convert('RGB')
# Strip the device chrome: status bar at the top, gesture pill at the bottom.
art = art.crop((0, int(art.height * 0.045), art.width, int(art.height * 0.955)))
cream = art.getpixel((30, int(art.height * 0.5)))     # sample the washi ground

# Right side: cover-crop the art into the remaining panel.
pw, ph = W - int(W * SPLIT), H
scale = max(pw / art.width, ph / art.height)
rs = art.resize((int(art.width * scale), int(art.height * scale)), Image.LANCZOS)
left = (rs.width - pw) // 2
top = int(rs.height * 0.42) - ph // 2
top = max(0, min(top, rs.height - ph))
panel = rs.crop((left, top, left + pw, top + ph))

img = Image.new('RGB', (W, H), cream)
img.paste(panel, (int(W * SPLIT), 0))

# Feather the seam so the print reads as ink spreading, not a pasted box.
seam = 90
px = img.load()
x0 = int(W * SPLIT)
for x in range(x0, min(x0 + seam, W)):
    a = (x - x0) / seam
    for y in range(H):
        r, g, b = px[x, y]
        px[x, y] = (int(cream[0]*(1-a) + r*a),
                    int(cream[1]*(1-a) + g*a),
                    int(cream[2]*(1-a) + b*a))

d = ImageDraw.Draw(img)
ink = (58, 54, 48)
def font(path, size):
    try: return ImageFont.truetype(path, size)
    except Exception: return ImageFont.load_default()

f_name = font('/System/Library/Fonts/Avenir Next.ttc', 76)
f_tag  = font('/System/Library/Fonts/Avenir Next.ttc', 27)
f_sub  = font('/System/Library/Fonts/Avenir Next.ttc', 22)

d.text((58, 168), 'midi-sink', font=f_name, fill=ink)
d.text((62, 262), 'Ink marbling, played with MIDI', font=f_tag, fill=(96, 90, 80))
d.text((62, 300), 'and an MPE instrument of its own', font=f_sub, fill=(130, 123, 112))

img.save(OUT)
print('wrote', OUT, img.size, 'alpha:', 'A' in img.mode)
