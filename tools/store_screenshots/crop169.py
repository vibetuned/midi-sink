#!/usr/bin/env python3
"""Crop a 2960x1848 (16:10) tablet capture to exactly 16:9 = 2960x1665,
taking the band out of the system chrome: status bar off the top, gesture
pill off the bottom. Play's large-screen slots reject anything squarer."""
from PIL import Image
import sys, os

TOP = 110          # status bar
TARGET_H = 1665    # 2960 / 16 * 9

def crop(src, dst):
    im = Image.open(src).convert('RGB')
    w, h = im.size
    assert (w, h) == (2960, 1848), f'{src}: unexpected {w}x{h}'
    out = im.crop((0, TOP, w, TOP + TARGET_H))
    out.save(dst)
    print(f'{os.path.basename(dst):28} {out.size[0]}x{out.size[1]}  ratio {out.size[0]/out.size[1]:.4f}')

if __name__ == '__main__':
    for src, dst in zip(sys.argv[1::2], sys.argv[2::2]):
        crop(src, dst)
