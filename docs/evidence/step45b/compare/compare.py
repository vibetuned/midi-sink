# The 44a compare, re-done: glass level, glow colour, where the strikes lie (200 px tiles).
import sys; from PIL import Image; import numpy as np
def stats(p):
    a = np.asarray(Image.open(p).convert("RGB")).astype(float) / 255.0; lum = a.mean(axis=2)
    lit = lum > 0.25; glass = ~lit
    tiles = sorted({(int(x) // 200, int(y) // 200) for y, x in zip(*np.where(lit))})
    occ = [t for t in tiles if lit[t[1]*200:(t[1]+1)*200, t[0]*200:(t[0]+1)*200].mean() > 0.002]
    rgb = tuple(int(v) for v in (a[lit].mean(axis=0) * 255)) if lit.any() else None
    return a.shape[1], a.shape[0], lum[glass].mean() * 255, 100 * lit.mean(), rgb, occ
for p in sys.argv[1:]:
    w, h, g, share, rgb, occ = stats(p)
    print(f"{p.split('/')[-1]} {w}x{h} glass mean {g:.2f} lit share {share:.3f}% lit RGB {rgb} occupied 200px tiles {occ}")
