"""Old fixture (absolute u,v) vs new dump (displacement dx,dy): ink/aux must be bitwise;
dx + st must agree with u to within the OLD payload's half-float rounding."""
import struct, sys, math
def load(p):
    b = open(p, "rb").read(); w, h = struct.unpack_from("<II", b, 0)
    n = w * h * 4; return w, h, struct.unpack_from("<%df" % n, b, 8)
def ulp_half(x):
    x = abs(x)
    if x < 2 ** -14: return 2 ** -24
    return 2 ** (math.floor(math.log2(x)) - 10)
w, h, old = load(sys.argv[1]); w2, h2, new = load(sys.argv[2]); assert (w, h) == (w2, h2)
ink_diff = aux_diff = 0; maxu = maxv = 0.0; over = 0; sumd = 0.0; maxd = 0.0
for y in range(h):
    sy = (y + 0.5) / h
    for x in range(w):
        i = (y * w + x) * 4; sx = (x + 0.5) / w
        if old[i + 2] != new[i + 2]: ink_diff += 1
        if old[i + 3] != new[i + 3]: aux_diff += 1
        du = abs((new[i] + sx) - old[i]); dv = abs((new[i + 1] + sy) - old[i + 1])
        maxu = max(maxu, du); maxv = max(maxv, dv)
        tol = 0.5 * ulp_half(old[i]) + 1e-6, 0.5 * ulp_half(old[i + 1]) + 1e-6
        if du > tol[0] or dv > tol[1]: over += 1
        d = math.hypot(new[i], new[i + 1]); sumd += d; maxd = max(maxd, d)
print(f"{w}x{h}: ink bitwise-differs {ink_diff}, aux bitwise-differs {aux_diff}")
print(f"pre-image agreement: max |du| {maxu:.3e}, max |dv| {maxv:.3e}; texels beyond half an old ULP: {over}")
print(f"new payload magnitude: mean |d| {sumd / (w * h):.3e}, max |d| {maxd:.3e} canvas")
