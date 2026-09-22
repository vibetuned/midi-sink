#!/usr/bin/env python3
# 2-D viscous multipole burst — derivation check (pure Python), the sibling of
# stokeslet_verify.py (MEDIUM_SPEC §2.3, Phase 6 step 38).
#
# Method after A. Jaffer, "The Lamb–Oseen Vortex and Paint Marbling"
# (arXiv:1810.04646): integrate a viscous flow over time into a closed-form
# displacement. Here the flow is the m-th viscous multipole.
#
# Vorticity of the m-th multipole heat kernel (∂_z^m of the Gaussian):
#   ω_m(r,θ,τ) = K · s^{m+1} e^{−s} / r^{m+2} · sin(mθ),      s = r²/(4ντ)
# Stream function, ∇²ψ = −ω, mode m (the Green's function r^m / r^{−m}):
#   ψ_m = (K (m−1)!/4) · γ_m(s) / r^m · sin(mθ),   γ_m(s) = P(m,s) = 1 − e^{−s} Σ_{k<m} s^k/k!
#   (m = 1 is the 2-D Stokeslet: ψ = (F/ρ) sinθ (1 − e^{−s})/(2πr); m = 0 Lamb–Oseen)
# Time integral Ψ = ∫₀ᵗ ψ dτ, with S = r²/(4νt):
#   ∫₀ᵗ γ_m(r²/4ντ) dτ = (r²/4ν) · Φ_m(S),    Φ_m(S) = ∫_S^∞ γ_m(s)/s² ds
#   by parts:  Φ_m(S) = γ_m(S)/S + Γ(m−1, S)/(m−1)!
#     m = 1:  Γ(0, S) = E1(S)                       — logarithmic kernel, NOT elementary
#     m ≥ 2:  Γ(m−1,S)/(m−1)! = e^{−S} Σ_{k≤m−2} S^k/k! / (m−1)   — elementary
#             = (1/S)[1 − e^{−S} Σ_{k=0}^{m−2} (1 − k/(m−1)) S^k/k!]  (the spec's form)
#     m = 2:  Φ_2(S) = χ(S) = (1 − e^{−S})/S
#   small S:  Φ_m(S) = 1/(m−1) − (1/(m−1)!) Σ_n (−1)^n S^{m+n−1} / (n! (m+n)(m+n−1))
# Dual-time Gaussian core — the impulse as a blob of radius a (t₀ = a²/4ν),
# aged from ℓ₀ to ℓ₁ (ℓ² = a² + 4νt):  Ψ_blob = Ψ_pt(t₁) − Ψ_pt(t₀)
#   Ψ_m = A_m (a/r)^{m−2} sin(mθ') [Φ_m(S₁) − Φ_m(S₀)],   S₀ = r²/ℓ₀², S₁ = r²/ℓ₁², θ' = θ − θ₀
#   d = ((1/r) ∂_θ Ψ, −∂_r Ψ):
#   d_r = (m A_m / r)(a/r)^{m−2} cos(mθ') ΔΦ
#   d_θ = (A_m / r)(a/r)^{m−2} sin(mθ') [(m−2) ΔΦ + 2(γ_m(S₁)/S₁ − γ_m(S₀)/S₀)]
# Normalisation: D = the lobe displacement at r = a on the ejection axis θ = θ₀:
#   A_m = D a / (m ΔΦ(r = a)).
import math, sys

FAILS = []
def check(ok, msg):
    print(("  ok    " if ok else "  FAIL  ") + msg)
    if not ok: FAILS.append(msg)

def E1(x):
    if x <= 1.0:
        s, term, k = 0.0, 1.0, 1
        while True:
            term *= -x / k; add = -term / k; s += add; k += 1
            if abs(add) < 1e-17 * max(1.0, abs(s)) or k > 200: break
        return -0.5772156649015329 - math.log(x) + s
    tiny = 1e-300; b = x + 1.0; c = 1.0 / tiny; d = 1.0 / b; h = d
    for i in range(1, 300):
        an = -i * i; b += 2.0
        d = 1.0 / (an * d + b); c = b + an / c; delta = c * d; h *= delta
        if abs(delta - 1.0) < 1e-16: break
    return h * math.exp(-x)

def P_reg(m, S):
    """γ_m(S) = P(m,S), the regularised lower incomplete gamma, m ≥ 1 integer."""
    if S <= 0.0: return 0.0
    if S < 1.0:                       # series e^{-S} Σ_{k≥m} S^k/k!  (no cancellation)
        term = S ** m / math.factorial(m); acc = term; k = m
        while term > 1e-18 * acc:
            k += 1; term *= S / k; acc += term
        return math.exp(-S) * acc
    return 1.0 - math.exp(-S) * sum(S ** k / math.factorial(k) for k in range(m))

def Gamma_upper_reg(n, S):
    """Γ(n,S)/(n−1)! for integer n ≥ 1  =  e^{-S} Σ_{k<n} S^k/k!."""
    return math.exp(-S) * sum(S ** k / math.factorial(k) for k in range(n))

def Phi_deficit(m, S):
    """1/(m−1) − Φ_m(S) = (1/(m−1)!) Σ_n (−1)^n S^{m+n−1}/(n!(m+n)(m+n−1)), 9 terms (< 1e-14 at S ≤ 0.05)."""
    acc = 0.0
    for n in range(0, 9):
        acc += (-1) ** n * S ** (m + n - 1) / (math.factorial(n) * (m + n) * (m + n - 1))
    return acc / math.factorial(m - 1)
def Phi_series(m, S): return 1.0 / (m - 1) - Phi_deficit(m, S)
def Phi_closed(m, S):
    """Φ_m(S) = γ_m(S)/S + Γ(m−1,S)/(m−1)!  (m ≥ 2)."""
    return P_reg(m, S) / S + Gamma_upper_reg(m - 1, S) / (m - 1)
def Phi(m, S):
    """Φ_m(S) = ∫_S^∞ γ_m(s)/s² ds: the closed form (m ≥ 2, series below S = 0.05), or χ + E1 for m = 1."""
    if m == 1: return (1.0 - math.exp(-S)) / S + E1(S)
    return Phi_series(m, S) if S < 0.05 else Phi_closed(m, S)

def Phi_spec(m, S):
    """The spec's elementary form: (1/S)[1 − e^{−S} Σ_{k=0}^{m−2} (1 − k/(m−1)) S^k/k!]."""
    return (1.0 / S) * (1.0 - math.exp(-S) * sum((1.0 - k / (m - 1)) * S ** k / math.factorial(k) for k in range(m - 1)))

def G_over_S(m, S):
    """γ_m(S)/S, stable at small S."""
    if S < 0.05:
        term = S ** (m - 1) / math.factorial(m); acc = term; k = m
        while term > 1e-18 * acc:
            k += 1; term *= S / k; acc += term
        return math.exp(-S) * acc
    return P_reg(m, S) / S

def simpson(f, a, b, n=20000):
    h = (b - a) / n; tot = f(a) + f(b)
    for i in range(1, n): tot += (4 if i % 2 else 2) * f(a + i * h)
    return tot * h / 3.0

# ---------------------------------------------------------------------------
print("== 1. Φ_m(S) closed form vs the numerical integral ∫_S^∞ γ_m(s)/s² ds  (tail beyond S+80: 1/(S+80))")
for m in (1, 2, 3, 4, 6):
    worst = 0.0
    for S in (0.02, 0.3, 1.0, 2.5, 7.0):
        num = simpson(lambda u: P_reg(m, math.exp(u)) / math.exp(u), math.log(S), math.log(S + 80.0)) + 1.0 / (S + 80.0)
        worst = max(worst, abs(num - Phi(m, S)) / abs(num))
    check(worst < 1e-8, f"m={m}: max relative error {worst:.1e}" + ("   (Φ_1 = χ + E1: the E1 kernel)" if m == 1 else ""))
print("== 1b. the spec's elementary form equals it for m ≥ 2, and Φ_2 = χ")
for m in (2, 3, 4, 5, 8):
    worst = max(abs(Phi_spec(m, S) - Phi(m, S)) / abs(Phi(m, S)) for S in (0.001, 0.05, 0.3, 1.0, 4.0, 12.0))
    check(worst < 1e-10, f"m={m}: |Φ_spec − Φ|/Φ ≤ {worst:.1e}")
check(max(abs(Phi(2, S) - (1 - math.exp(-S)) / S) for S in (0.01, 0.5, 3.0)) < 1e-12, "Φ_2(S) = χ(S) = (1 − e^{−S})/S")
print("== 1c. the remainder after integrating by parts: E1 for m = 1 (non-elementary), an exponential polynomial for m ≥ 2")
for m in (1, 2, 3):
    S = 0.7
    rem = simpson(lambda u: P_reg(m, math.exp(u)) / math.exp(u), math.log(S), math.log(S + 80.0)) + 1.0 / (S + 80.0) - P_reg(m, S) / S
    if m == 1: check(abs(rem - E1(S)) < 1e-6, f"m=1: Φ_1 − γ_1/S = {rem:.8f} = E1(S) = {E1(S):.8f}")
    else:
        el = Gamma_upper_reg(m - 1, S) / (m - 1)
        check(abs(rem - el) < 1e-6, f"m={m}: Φ_m − γ_m/S = {rem:.8f} = e^(−S) Σ_(k≤m−2) S^k/k! /(m−1) = {el:.8f}")
print("== 1d. Φ_m(0) = 1/(m−1): the diffused-zone plateau; the small-S series matches the closed form at the switch (S = 0.05)")
for m in (2, 3, 5):
    dev = max(abs(Phi_series(m, S) - Phi_closed(m, S)) / Phi_closed(m, S) for S in (0.02, 0.05, 0.08))
    check(dev < 1e-13 and abs(Phi(m, 1e-9) - 1.0 / (m - 1)) < 1e-9, f"m={m}: Φ_m(0) = {Phi(m, 1e-9):.9f}; series vs closed form at S = 0.02 / 0.05 / 0.08: relative deviation ≤ {dev:.1e}")

# ---------------------------------------------------------------------------
print("== 2. ∇²ψ_m = −ω_m for ψ_m = (K (m−1)!/4) γ_m(s) r^{−m} sin(mθ), ω_m = K s^{m+1} e^{−s} r^{−m−2} sin(mθ)   (4ντ = 1, K = 1)")
def psi_pt(m, x, y):
    r2 = x * x + y * y; r = math.sqrt(r2); s = r2
    return 0.25 * math.factorial(m - 1) * P_reg(m, s) / r ** m * math.sin(m * math.atan2(y, x))
def omega(m, x, y):
    r2 = x * x + y * y; r = math.sqrt(r2); s = r2
    return s ** (m + 1) * math.exp(-s) / r ** (m + 2) * math.sin(m * math.atan2(y, x))
for m in (1, 2, 3, 4):
    worst = 0.0
    for (x, y) in ((0.3, 0.2), (0.9, -0.4), (1.4, 1.1), (0.15, 0.7)):
        h = 1e-4
        lap = (psi_pt(m, x + h, y) + psi_pt(m, x - h, y) + psi_pt(m, x, y + h) + psi_pt(m, x, y - h) - 4 * psi_pt(m, x, y)) / (h * h)
        worst = max(worst, abs(lap + omega(m, x, y)) / max(1e-12, abs(omega(m, x, y))))
    check(worst < 1e-5, f"m={m}: max |∇²ψ + ω|/|ω| = {worst:.1e}")
print("== 2b. ω_m solves the vorticity diffusion equation ∂_τ ω = ν ∇²ω  (ν = 1/4 so 4ντ = τ)")
def omega_t(m, x, y, tau):
    r2 = x * x + y * y; r = math.sqrt(r2); s = r2 / tau
    return s ** (m + 1) * math.exp(-s) / r ** (m + 2) * math.sin(m * math.atan2(y, x))
for m in (2, 3):
    worst = 0.0
    for (x, y) in ((0.5, 0.3), (1.1, -0.6)):
        tau, h, ht = 1.0, 1e-4, 1e-5
        dt = (omega_t(m, x, y, tau + ht) - omega_t(m, x, y, tau - ht)) / (2 * ht)
        lap = (omega_t(m, x + h, y, tau) + omega_t(m, x - h, y, tau) + omega_t(m, x, y + h, tau) + omega_t(m, x, y - h, tau) - 4 * omega_t(m, x, y, tau)) / (h * h)
        worst = max(worst, abs(dt - 0.25 * lap) / max(1e-12, abs(dt)))
    check(worst < 1e-5, f"m={m}: max |∂_τ ω − ν∇²ω|/|∂_τ ω| = {worst:.1e}")

# ---------------------------------------------------------------------------
print("== 3. the time integral: ∫₀ᵗ γ_m(r²/4ντ) dτ = (r²/4ν) Φ_m(r²/4νt)   (numerical midpoint, 4ν = 1)")
for m in (1, 2, 3, 5):
    worst = 0.0
    for (r, t) in ((0.6, 1.0), (1.5, 0.5), (0.2, 2.0)):
        n = 200000; h = t / n
        num = sum(P_reg(m, r * r / ((i + 0.5) * h)) for i in range(n)) * h
        cl = r * r * Phi(m, r * r / t)
        worst = max(worst, abs(num - cl) / cl)
    check(worst < 1e-6, f"m={m}: max relative error {worst:.1e}")

# ---------------------------------------------------------------------------
# The blob kernel (dual time), in the burst frame (θ' = θ − θ₀).
def dPhi(m, S0, S1):
    """Φ_m(S₁) − Φ_m(S₀), S₁ < S₀. Near the core both sit at the 1/(m−1) plateau: difference the
    DEFICITS there (no constant to round away) — the form the shader uses."""
    if S0 < 0.05: return Phi_deficit(m, S0) - Phi_deficit(m, S1)
    return Phi(m, S1) - Phi(m, S0)
def Psi_blob(m, A, a, l0, l1, x, y, th0=0.0):
    r2 = x * x + y * y; r = math.sqrt(r2)
    if r2 < 1e-300: return 0.0
    thp = math.atan2(y, x) - th0
    return A * (a / r) ** (m - 2) * math.sin(m * thp) * dPhi(m, r2 / (l0 * l0), r2 / (l1 * l1))
def d_blob(m, A, a, l0, l1, x, y, th0=0.0):
    """Closed-form displacement (d_x, d_y) of the burst: the impulse of core a aged from ℓ₀ to ℓ₁."""
    r2 = x * x + y * y; r = math.sqrt(r2)
    if r2 < 1e-300: return (0.0, 0.0)
    th = math.atan2(y, x); thp = th - th0
    S0, S1 = r2 / (l0 * l0), r2 / (l1 * l1)
    dph = dPhi(m, S0, S1)
    pre = A / r * (a / r) ** (m - 2)
    dr = m * pre * math.cos(m * thp) * dph
    dth = pre * math.sin(m * thp) * ((m - 2) * dph + 2.0 * (G_over_S(m, S1) - G_over_S(m, S0)))
    return (dr * math.cos(th) - dth * math.sin(th), dr * math.sin(th) + dth * math.cos(th))
def lobe_D(m, A, a, l0, l1):
    """The lobe displacement at r = a on the axis (d_r there)."""
    return m * A / a * dPhi(m, 1.0 * a * a / (l0 * l0), a * a / (l1 * l1))
def A_from_D(m, D, a, l0, l1): return D * a / (m * dPhi(m, a * a / (l0 * l0), a * a / (l1 * l1)))

print("== 4. d = ((1/r)∂_θΨ, −∂_rΨ) — the closed-form d_r/d_θ against finite differences of Ψ; div d = 0; d(0) = 0")
a = 0.03
for m in (2, 3, 4):
    A = A_from_D(m, 0.01, a, a, 4 * a)
    worst_d = worst_div = 0.0; wp = None
    for (x, y) in ((0.004, 0.002), (0.02, 0.015), (0.05, -0.03), (0.1, 0.08), (0.25, 0.05), (0.4, -0.3)):
        h = 1e-6 * (a + math.hypot(x, y))
        fd = ((Psi_blob(m, A, a, a, 4 * a, x, y + h) - Psi_blob(m, A, a, a, 4 * a, x, y - h)) / (2 * h),
              -(Psi_blob(m, A, a, a, 4 * a, x + h, y) - Psi_blob(m, A, a, a, 4 * a, x - h, y)) / (2 * h))
        cl = d_blob(m, A, a, a, 4 * a, x, y)
        e = math.hypot(fd[0] - cl[0], fd[1] - cl[1]) / math.hypot(*cl)
        if e > worst_d: worst_d = e; wp = (x, y)
        dxx = (d_blob(m, A, a, a, 4 * a, x + h, y)[0] - d_blob(m, A, a, a, 4 * a, x - h, y)[0]) / (2 * h)
        dyx = (d_blob(m, A, a, a, 4 * a, x + h, y)[1] - d_blob(m, A, a, a, 4 * a, x - h, y)[1]) / (2 * h)
        dxy = (d_blob(m, A, a, a, 4 * a, x, y + h)[0] - d_blob(m, A, a, a, 4 * a, x, y - h)[0]) / (2 * h)
        dyy = (d_blob(m, A, a, a, 4 * a, x, y + h)[1] - d_blob(m, A, a, a, 4 * a, x, y - h)[1]) / (2 * h)
        worst_div = max(worst_div, abs(dxx + dyy) / math.sqrt(dxx * dxx + dxy * dxy + dyx * dyx + dyy * dyy))
    check(worst_d < 1e-7 and worst_div < 1e-7 and d_blob(m, A, a, a, 4 * a, 0.0, 0.0) == (0.0, 0.0),
          f"m={m}: |d_closed − ∇⊥Ψ|/|d| ≤ {worst_d:.1e} (worst at {wp});  |div d| / ‖∇d‖ ≤ {worst_div:.1e};  d(0) = (0, 0) — the stagnation origin")
print("== 4b. the normalisation: d_r(a, θ₀) = D exactly; sign: D > 0 ejects along θ₀ (d_r > 0) and draws in along θ₀ ± π/2")
for m in (2, 3):
    D = 0.01; th0 = 0.7
    A = A_from_D(m, D, a, a, 4 * a)
    on = d_blob(m, A, a, a, 4 * a, a * math.cos(th0), a * math.sin(th0), th0)
    dr_on = on[0] * math.cos(th0) + on[1] * math.sin(th0)
    tin = th0 + math.pi / m
    inn = d_blob(m, A, a, a, 4 * a, a * math.cos(tin), a * math.sin(tin), th0)
    dr_in = inn[0] * math.cos(tin) + inn[1] * math.sin(tin)
    check(abs(dr_on - D) < 1e-12 and abs(dr_in + D) < 1e-12, f"m={m}, θ₀ = {th0}: d_r(a, θ₀) = {dr_on:.6f} (D = {D}); d_r(a, θ₀ + π/m) = {dr_in:.6f}")

# ---------------------------------------------------------------------------
print("== 5. r → 0: the quadrupole is pure hyperbolic strain d ≈ λ(x', −y'), λ = A₂(1/a² − 1/ℓ²) — the pinch; order m is the harmonic polynomial Im(z^m): |d| ∝ r^{m−1}")
for l_over_a in (2.0, 4.0, 50.0):
    l = l_over_a * a; D = 0.01
    A = A_from_D(2, D, a, a, l)
    lam = A * (1.0 / (a * a) - 1.0 / (l * l))
    worst = 0.0
    for (x, y) in ((0.002, 0.001), (0.001, -0.0015), (0.0005, 0.0005)):
        d = d_blob(2, A, a, a, l, x, y)
        worst = max(worst, math.hypot(d[0] - lam * x, d[1] + lam * y) / math.hypot(d[0], d[1]))
    check(worst < 5e-3, f"ℓ/a = {l_over_a}: |d − λ(x, −y)|/|d| ≤ {worst:.1e} at r ≤ 0.075 a;  λ = {lam / (D / a):.4f} (D/a)  [ℓ→∞: 1/(2(1 − χ(1))) = {1 / (2 * (1 - (1 - math.exp(-1)))):.4f}]")
for m in (3, 4):
    A = A_from_D(m, 0.01, a, a, 4 * a)
    d1 = math.hypot(*d_blob(m, A, a, a, 4 * a, 0.001, 0.0004)); d2 = math.hypot(*d_blob(m, A, a, a, 4 * a, 0.002, 0.0008))
    check(abs(d2 / d1 - 2 ** (m - 1)) < 0.02 * 2 ** (m - 1), f"m={m}: |d(2r)|/|d(r)| = {d2 / d1:.4f} ≈ 2^(m−1) = {2 ** (m - 1)} near the core")

print("== 6. the far field: the diffused zone a ≪ r ≪ ℓ decays as cos(mθ')/r^{m−1} (the quadrupole: cos 2θ'/r); beyond ℓ as 1/r^{m+1}")
def slope(m, A, a, l0, l1, r, th=0.0):
    f = lambda rr: math.hypot(*d_blob(m, A, a, l0, l1, rr * math.cos(th), rr * math.sin(th)))
    return (math.log(f(r * 1.01)) - math.log(f(r / 1.01))) / (2 * math.log(1.01))
for m in (2, 3, 4):
    l = 400.0 * a; A = A_from_D(m, 0.01, a, a, l)
    s_in = slope(m, A, a, a, l, 20.0 * a); s_out = slope(m, A, a, a, l, 8.0 * l)
    check(abs(s_in + (m - 1)) < 0.03 and abs(s_out + (m + 1)) < 0.03,
          f"m={m}, ℓ = 400a: d log|d| / d log r at r = 20a: {s_in:+.3f} (→ −{m - 1}); at r = 8ℓ: {s_out:+.3f} (→ −{m + 1})")
l = 60 * a; A = A_from_D(2, 0.01, a, a, l); r = 10 * a
corr = 1.0 - (a / r) ** 2 - 0.5 * (r / l) ** 2          # χ(S₀) ≈ 1/S₀, χ(S₁) ≈ 1 − S₁/2
angs = []
for t in (0.0, 0.4, 1.0, 1.9, 2.8):
    d = d_blob(2, A, a, a, l, r * math.cos(t), r * math.sin(t))
    angs.append(abs(d[0] * math.cos(t) + d[1] * math.sin(t) - 2 * A / r * math.cos(2 * t) * corr))
check(max(angs) < 2e-4 * 2 * A / r, f"quadrupole at r = 10a, ℓ = 60a: d_r = (2A₂/r) cos 2θ' (1 − a²/r² − r²/2ℓ²) to {max(angs) / (2 * A / r):.1e} of 2A₂/r")

print("== 7. where the lobe peaks along the axis (the spec normalises at r = a): r_peak/a and |d(r_peak)|/|d(a)| per age")
for m in (2, 3):
    for l_over_a in (1.5, 2.0, 4.0, 8.0, 12.0):
        l = l_over_a * a; A = A_from_D(m, 0.01, a, a, l)
        best = (0.0, 0.0)
        for i in range(1, 2000):
            r = a * (0.01 + i * 0.01)
            v = abs(d_blob(m, A, a, a, l, r, 0.0)[0])
            if v > best[1]: best = (r, v)
        print(f"        m={m}, ℓ/a = {l_over_a:4}: peak at r = {best[0] / a:.2f} a, |d| there = {best[1] / 0.01:.3f} D")

print("== 8. fold budget of one pass (an increment ℓ₀ → ℓ₁ of the impulse of core a). Two normalisations of max|∂d_i/∂x_j|:")
print("       per (D/a), D = the lobe displacement at r = a (the API's amplitude)  |  per (d_max/ℓ₀), d_max = the pass's peak |d|, ℓ₀ its current core")
print("       inverse lookup P_src = P − d: det = (1 − d_xx)(1 − d_yy) − d_xy d_yx.  Also: the peak |d| sits on the ejection axis (axis max / global max).")
def pass_table(m, l0, l1, a=1.0):
    A = A_from_D(m, 1.0, a, l0, l1)          # D = 1, a = 1
    g = 0.0; where = None; dmax = 0.0; dmax_axis = 0.0; Js = []
    rs = [a * 10 ** (-2 + 4.0 * i / 100) for i in range(101)]
    rs = [r for r in rs if r <= 6 * l1] + [6 * l1]
    for r in rs:
        for j in range(48):
            th = j * math.pi / 24
            x, y = r * math.cos(th), r * math.sin(th)
            h = 1e-6 * (r + a)
            dxx = (d_blob(m, A, a, l0, l1, x + h, y)[0] - d_blob(m, A, a, l0, l1, x - h, y)[0]) / (2 * h)
            dyx = (d_blob(m, A, a, l0, l1, x + h, y)[1] - d_blob(m, A, a, l0, l1, x - h, y)[1]) / (2 * h)
            dxy = (d_blob(m, A, a, l0, l1, x, y + h)[0] - d_blob(m, A, a, l0, l1, x, y - h)[0]) / (2 * h)
            dyy = (d_blob(m, A, a, l0, l1, x, y + h)[1] - d_blob(m, A, a, l0, l1, x, y - h)[1]) / (2 * h)
            gg = max(abs(dxx), abs(dyy), abs(dxy), abs(dyx))
            if gg > g: g = gg; where = (r / a, th)
            Js.append((dxx, dxy, dyx, dyy))
            dd = math.hypot(*d_blob(m, A, a, l0, l1, x, y))
            dmax = max(dmax, dd)
            if j == 0: dmax_axis = max(dmax_axis, dd)
    return g, where, dmax, dmax_axis, Js
rows = ((1.0, 1.05), (1.0, 1.5), (1.0, 2.0), (1.0, 4.0), (1.0, 8.0), (1.0, 12.0), (2.0, 2.2), (4.0, 4.4), (8.0, 8.8), (4.0, 8.0), (11.0, 12.0))
g_core = {}
axis_ratio_min = 1.0
for m in (2, 3, 4, 5, 6, 7, 8):
    for (l0, l1) in rows:
        g, where, dmax, dmax_axis, Js = pass_table(m, l0, l1)
        gc = g / (dmax / l0)
        g_core[m] = max(g_core.get(m, 0.0), gc)
        axis_ratio_min = min(axis_ratio_min, dmax_axis / dmax)
        print(f"        m={m}, ℓ₀/a = {l0:4} → ℓ₁/a = {l1:5}: max|∂d| = {g:8.3f} (D/a) at r = {where[0]:6.2f} a  |  {gc:.3f} (d_max/ℓ₀), d_max = {dmax:8.3f} D at axis ratio {dmax_axis / dmax:.3f}")
print("       the worst per (d_max/ℓ₀) per order: " + ", ".join(f"m={m}: {g_core[m]:.3f}" for m in sorted(g_core)))
gw = max(g_core.values())
beta = 0.25 / gw
print("       budget rule per order (|∇d| ≤ 0.25): d_max ≤ β_m ℓ₀ with β_m = " + ", ".join(f"m={m}: {0.25 / g_core[m]:.3f}" for m in sorted(g_core)))
print(f"       one rule for every order and age in the table: d_max ≤ {beta:.3f} ℓ₀")
check(g_core[2] < 2.0 and gw < 4.0, f"the core-normalised gradient is bounded: quadrupole ≤ {g_core[2]:.3f}, all orders ≤ {gw:.3f} (d_max/ℓ₀)")
check(axis_ratio_min > 0.999, f"the peak |d| of every pass lies on the ejection axis (axis max / global max ≥ {axis_ratio_min:.4f})")
# the det floor under the rule, on the sharpest quadrupole pass and the worst row overall
for m, (l0, l1) in ((2, (1.0, 1.05)), (2, (1.0, 12.0)), (4, (1.0, 1.05)), (6, (1.0, 1.05))):
    g, where, dmax, dmax_axis, Js = pass_table(m, l0, l1)
    scale = beta * l0 / dmax                  # the D that puts d_max at the budget
    detmin = min((1 - scale * dxx) * (1 - scale * dyy) - scale * scale * dxy * dyx for (dxx, dxy, dyx, dyy) in Js)
    check(detmin > 0.5, f"m={m}, ℓ₀/a = {l0} → {l1}: at the budget (d_max = {beta:.3f} ℓ₀) the inverse-lookup det stays ≥ {detmin:.3f} (> 0.5)")
print("       for the quadrupole the sharpest pass (ℓ₀ = a → 1.05 a) is the strictest: its d_max = 1.007 D, so at ℓ₀ = a the API amplitude per pass is D ≤ %.3f a under its own β₂" % (0.25 / g_core[2] / 1.007))

print()
print("SUMMARY: " + ("all checks passed" if not FAILS else f"{len(FAILS)} FAILED: " + "; ".join(FAILS)))
sys.exit(1 if FAILS else 0)
