import sys; from PIL import Image; import numpy as np
def label(m):
    h, w = m.shape; lab = np.zeros((h, w), int); n = 0
    for y in range(h):
        for x in range(w):
            if m[y, x] and not lab[y, x]:
                n += 1; st = [(y, x)]; lab[y, x] = n
                while st:
                    cy, cx = st.pop()
                    for dy, dx in ((1,0),(-1,0),(0,1),(0,-1)):
                        yy, xx = cy + dy, cx + dx
                        if 0 <= yy < h and 0 <= xx < w and m[yy, xx] and not lab[yy, xx]: lab[yy, xx] = n; st.append((yy, xx))
    return lab, n
for p in sys.argv[1:]:
    a = np.asarray(Image.open(p).convert("L").resize((370, 231))).astype(float); lab, n = label(a > 40)
    cs = [(int((lab == i).sum()), tuple(int(v) for v in np.argwhere(lab == i).mean(0)[::-1])) for i in range(1, n + 1)]
    cs.sort(reverse=True); print(p.split("/")[-1], "charges:", n, [c for c in cs if c[0] > 3])
