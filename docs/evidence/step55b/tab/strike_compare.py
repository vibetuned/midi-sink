#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "pillow", "scipy"]
# ///
"""DECISIONS_5 #88's judgement, repeated for step 55b: the six-strike prints of
the Mac (`midi-sink --dev --anod-strike-render <dir>`, PNG) and the Tab
(`adb shell am start ... --es strikes 1 [--ef sparkShear 2.0]`, raw
w,h + RGBA8) compared by LIT BLOBS — connected components of pixels whose
luminance stands above the glass (the print's median) by more than `--lit`
(60/255: the charges' cores and halos, not the water grid's lines, which sit
under 40 over the glass at the author's defaults), the components under
`--min-px` pixels dropped as speckle. Prints: charge count, each charge's
pixels, the lit share.

  uv run strike_compare.py mac/anod_strikes.png tab/anod_strikes_default.rgba [--lit 60] [--min-px 200]
"""
import argparse
import struct
import sys

import numpy as np
from PIL import Image
from scipy import ndimage


def load(path):
    if path.endswith(".rgba"):
        with open(path, "rb") as f:
            w, h = struct.unpack("<II", f.read(8))
            px = np.frombuffer(f.read(), dtype=np.uint8).reshape(h, w, 4)
    else:
        px = np.asarray(Image.open(path).convert("RGBA"))
    return px


def lit_blobs(px, lit, min_px):
    rgb = px[..., :3].astype(np.float64)
    lum = 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]
    glass = np.median(lum)                       # the substrate: most of the sheet is dark glass
    mask = lum > glass + lit
    labels, n = ndimage.label(mask)
    sizes = ndimage.sum(mask, labels, range(1, n + 1)) if n else []
    sizes = sorted((int(s) for s in sizes if s >= min_px), reverse=True)
    return glass, mask.mean(), sizes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("prints", nargs="+")
    ap.add_argument("--lit", type=float, default=60.0)   # the charges stand 60+ levels over the glass; the water grid under 40
    ap.add_argument("--min-px", type=int, default=200)
    a = ap.parse_args()
    for p in a.prints:
        px = load(p)
        glass, share, sizes = lit_blobs(px, a.lit, a.min_px)
        print(f"{p}: {px.shape[1]}x{px.shape[0]}, glass {glass:.1f}/255, lit {100 * share:.2f} %, "
              f"{len(sizes)} charges >= {a.min_px} px: {sizes[:12]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
