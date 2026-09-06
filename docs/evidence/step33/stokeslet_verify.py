# 2-D unsteady Stokeslet displacement kernel — derivation check (pure Python).
#
# Impulse F x̂ δ(x) δ(t) in a 2-D incompressible Stokes layer (ρ, ν).
# Vorticity ω = -(F/ρ) ∂_y H,  H = heat kernel (1/4πντ) e^{-r²/4ντ}
# Stream function ψ = (F/ρ) y g(r,τ),  g = (1 - e^{-s}) / (2π r²),  s = r²/(4ντ)
# u_x = ∂_y ψ = (F/ρ)[g + y² g'/r],  u_y = -∂_x ψ = -(F/ρ) x y g'/r
# g'/r = [s e^{-s} - (1 - e^{-s})] / (π r⁴)
# Displacement d = ∫₀ᵗ u dτ  (Eulerian / linearized), with S = r²/(4νt):
#   ∫ g dτ      = (1/8πν) [ χ(S) + E1(S) ],      χ(S) = (1 - e^{-S})/S
#   ∫ g'/r dτ   = -(1/4πν) χ(S) / r²
#   d_x = (D0/8π)[χ+E1](S) - (D0/4π)(y²/r²) χ(S),   d_y = +(D0/4π)(xy/r²) χ(S),   D0 = F/(ρν)
# Gaussian-blob impulse of radius a (= heat spread a² = 4ν t0): d_a(t) = d_pt(t+t0) - d_pt(t0)
#   -> S0 = r²/a², S1 = r²/ℓ², ℓ² = a² + 4νt;  centre displacement = (D0/4π) ln(ℓ/a)
# Normalise so the tip moves by exactly d:  D0 = 4π d / ln(ℓ/a).
import math

def E1(x):
    """Exponential integral E1(x), x>0. Series for x<=1, continued fraction (Lentz) above."""
    if x <= 1.0:
        s, term, k = 0.0, 1.0, 1
        while True:
            term *= -x / k
            add = -term / k
            s += add
            k += 1
            if abs(add) < 1e-17 * max(1.0, abs(s)) or k > 200: break
        return -0.5772156649015329 - math.log(x) + s   # Σ (-1)^{k+1} x^k/(k k!)
    # continued fraction: E1(x) = e^{-x} / (x + 1/(1 + 1/(x + 2/(1 + 2/(x + ...)))))  (modified Lentz)
    tiny = 1e-300
    b = x + 1.0; c = 1.0 / tiny; d = 1.0 / b; h = d
    for i in range(1, 300):
        an = -i * i; b += 2.0
        d = 1.0 / (an * d + b); c = b + an / c; delta = c * d; h *= delta
        if abs(delta - 1.0) < 1e-16: break
    return h * math.exp(-x)

def E1_numeric(x, n=200000):
    # Simpson on e^{-s}/s from x to x+60 (tail beyond is < e^{-60})
    a, b = x, x + 60.0; hh = (b - a) / n
    f = lambda s: math.exp(-s) / s
    tot = f(a) + f(b)
    for i in range(1, n):
        tot += (4 if i % 2 else 2) * f(a + i * hh)
    return tot * hh / 3.0

def chi(S): return (1.0 - math.exp(-S)) / S if S > 1e-12 else 1.0 - S / 2.0

def d_point(x, y, D0, nu_t):          # displacement of the POINT impulse after time t (4νt = nu_t*... passes 4νt)
    r2 = x * x + y * y
    S = r2 / nu_t
    if r2 < 1e-300: return (float('inf'), 0.0)
    return (D0 / (8 * math.pi) * (chi(S) + E1(S)) - D0 / (4 * math.pi) * (y * y / r2) * chi(S),
            +D0 / (4 * math.pi) * (x * y / r2) * chi(S))

def d_blob(x, y, d, a, ell):
    """Regularised kernel: tip Gaussian radius a, spread ℓ after the impulse. Centre moves by d."""
    L = math.log(ell / a)
    D0 = 4 * math.pi * d / L
    r2 = x * x + y * y
    S0, S1 = r2 / (a * a), r2 / (ell * ell)
    if r2 < 1e-30:
        return (d, 0.0)
    dx = D0 / (8 * math.pi) * ((chi(S1) + E1(S1)) - (chi(S0) + E1(S0))) - D0 / (4 * math.pi) * (y * y / r2) * (chi(S1) - chi(S0))
    dy = +D0 / (4 * math.pi) * (x * y / r2) * (chi(S1) - chi(S0))
    return (dx, dy)

def u_point(x, y, D0nu, tau):          # velocity of the point impulse: D0ν = F/ρ ; 4ντ passed as tau4
    r2 = x * x + y * y; s = r2 / tau
    g = (1 - math.exp(-s)) / (2 * math.pi * r2)
    gpr = (s * math.exp(-s) - (1 - math.exp(-s))) / (math.pi * r2 * r2)
    return (D0nu * (g + y * y * gpr), -D0nu * x * y * gpr)

print("== E1 accuracy vs Simpson")
for x in (0.05, 0.5, 1.0, 2.0, 6.0):
    print(f"  E1({x}) = {E1(x):.10f}   numeric {E1_numeric(x):.10f}")

print("== d_point(t) equals ∫₀ᵗ u dτ (numerical, 4ν = 1 units)")
D0 = 1.0; T = 1.0
for (x, y) in ((0.6, 0.2), (0.1, 0.5), (1.5, -0.7)):
    n = 400000; h = T / n; sx = sy = 0.0
    for i in range(1, n + 1):
        tau = (i - 0.5) * h
        ux, uy = u_point(x, y, D0, tau)   # here D0ν := D0 (ν=1/4 so 4ν=1) -> u uses F/ρ = D0·ν = D0/4
        sx += ux * 0.25; sy += uy * 0.25
    ax, ay = d_point(x, y, D0, T)
    print(f"  ({x},{y}): closed ({ax:.6f},{ay:.6f})  numeric ({sx*h:.6f},{sy*h:.6f})")

print("== blob kernel: centre displacement, divergence, symmetry, far field")
a, ell, d = 0.03, 0.09, 0.01
print(f"  d_blob(0) = {d_blob(0,0,d,a,ell)}   (must be (d,0))")
eps = 1e-6; worst = 0.0
for (x, y) in ((0.01, 0.005), (0.03, 0.02), (0.06, -0.04), (0.12, 0.09), (0.3, 0.1)):
    dxdx = (d_blob(x+eps,y,d,a,ell)[0] - d_blob(x-eps,y,d,a,ell)[0]) / (2*eps)
    dydy = (d_blob(x,y+eps,d,a,ell)[1] - d_blob(x,y-eps,d,a,ell)[1]) / (2*eps)
    worst = max(worst, abs(dxdx + dydy))
print(f"  max |div d| over samples = {worst:.2e}  (divergence-free -> ~1e-9)")
print(f"  mirror: d(x,y)={d_blob(0.05,0.03,d,a,ell)}  d(x,-y)={d_blob(0.05,-0.03,d,a,ell)}")
r = 1.0; ff = d_blob(r, 0, d, a, ell)[0]; pred = d / math.log(ell/a) * (ell*ell - a*a) / (r*r) * 0.5
print(f"  far field on axis at r=1: {ff:.3e}  ~ d/ln(ℓ/a)·(ℓ²-a²)/(2r²) = {pred:.3e}  (1/r² doublet tail)")

print("== fold budget: max |∇d| per unit d/a, as a function of ℓ/a")
for ratio in (1.5, 2.0, 3.0, 5.0, 8.0):
    a = 1.0; ell = ratio; d = 1.0
    gmax = 0.0
    for i in range(-80, 81):
        for j in range(-80, 81):
            x, y = i * 0.05, j * 0.05
            if x == 0 and y == 0: continue
            e = 1e-5
            J = [[(d_blob(x+e,y,d,a,ell)[k] - d_blob(x-e,y,d,a,ell)[k])/(2*e) for k in (0,1)],
                 [(d_blob(x,y+e,d,a,ell)[k] - d_blob(x,y-e,d,a,ell)[k])/(2*e) for k in (0,1)]]
            # inverse-lookup map P_src = P - d(P): det = (1-dxx)(1-dyy) - dxy dyx ; fold when det <= 0
            g = max(abs(J[0][0]), abs(J[1][1]), abs(J[0][1]), abs(J[1][0]))
            gmax = max(gmax, g)
    print(f"  ℓ/a = {ratio}: max|∂d| = {gmax:.3f} per (d/a)  ->  fold-free needs d < {1/gmax:.2f} a; budget d ≤ {0.25/gmax:.3f} a keeps |∇d| ≤ 0.25")
