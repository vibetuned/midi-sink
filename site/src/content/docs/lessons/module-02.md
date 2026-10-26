---
title: "Module 2 — Hamiltonian vector fields and symplectic integrators"
description: "Hamiltonian flows and their splittings on the canvas (Chladni, the braid, the wake), the port-Hamiltonian Dirac network, symplectic Euler, Störmer–Verlet and RK4 compared, the shadow Hamiltonian at audio rate, the Toda ring's leapfrog in double, and the four ways a symplectic integrator still fails."
---
*Lecture notes written against `midi-sink` as shipped (libsumi 1.12.0, Voxo 0.26.0, 2026-10-25) and checked claim by claim against the engine before this fold (2026-10-26; libsumi is unchanged since, and Voxo's later versions add the sampler's second generation and touch no synth claim here). Code citations and gate numbers refer to `docs/PROJECT_SPEC.md` (spec v6), `docs/DECISIONS.md` Part XIII (cited `DECISIONS_13 #n`), and the Kessho proposal `medium_specs_v3.md` (in git history; its substance §10.7). The lab beside the module is a standalone teaching re-implementation, never the engine; the engine's own lab is [the Suzu lab](../../suzu/lab/).*

---

## Lecture Notes

### 2.1 From Energy Landscapes to Flows: The Hamiltonian Vector Field $X_H$

In Module 1, we built the phase space $M = T^*Q$ and endowed it with the canonical symplectic 2-form $\omega = -d\theta = \sum_{i=1}^n dq_i \wedge dp_i$. We proved that $\omega$ is **closed** ($d\omega = 0$) and **non-degenerate** ($\det \Omega \neq 0$). Now we put that geometry to work: how does a scalar energy function $H : M \to \mathbb{R}$ generate motion?

On a Riemannian manifold with metric $g$, a function $H$ induces the **gradient vector field** $\nabla H$ via $g(\nabla H, v) = dH(v)$. Trajectories of $\nabla H$ climb steepest-ascent slopes (or descend them under $-\nabla H$), so $H$ changes monotonically along the flow and every local minimum is a sink ($\operatorname{div}(-\nabla H) < 0$).

On a **symplectic manifold** $(M, \omega)$, we replace the symmetric metric $g$ with the skew-symmetric 2-form $\omega$. Because $\omega$ is non-degenerate, the bundle map $\omega^\flat : TM \to T^*M$ defined by $v \mapsto i_v\omega = \omega(v, \cdot)$ is a linear isomorphism at every point. Given any smooth Hamiltonian $H \in C^\infty(M)$, there exists a unique vector field $X_H$ — the **Hamiltonian vector field** of $H$ — satisfying:

$$i_{X_H}\omega = dH$$

Evaluating both sides on an arbitrary tangent vector $v$ means $\omega(X_H, v) = dH(v)$. Let us compute $X_H$ in canonical Darboux coordinates $(q_1, \dots, q_n, p_1, \dots, p_n)$. Write:

$$X_H = \sum_{i=1}^n \left( A_i \frac{\partial}{\partial q_i} + B_i \frac{\partial}{\partial p_i} \right), \qquad dH = \sum_{i=1}^n \left( \frac{\partial H}{\partial q_i}\,dq_i + \frac{\partial H}{\partial p_i}\,dp_i \right)$$

Taking the interior product of $X_H$ with $\omega = \sum_{i=1}^n dq_i \wedge dp_i$ using the anti-derivation rule from Module 1 (§1.1):

$$i_{X_H}\omega = \sum_{i=1}^n \Big( (i_{X_H}dq_i)\,dp_i - (i_{X_H}dp_i)\,dq_i \Big) = \sum_{i=1}^n \left( A_i\,dp_i - B_i\,dq_i \right)$$

Matching coefficients of $dq_i$ and $dp_i$ against $dH$ yields $A_i = \frac{\partial H}{\partial p_i}$ and $-B_i = \frac{\partial H}{\partial q_i}$, which are **Hamilton's equations**:

$$\dot{q}_i = \frac{\partial H}{\partial p_i}, \qquad \dot{p}_i = -\frac{\partial H}{\partial q_i} \qquad \Longleftrightarrow \qquad \dot{z} = J\,\nabla H(z), \quad J = \Omega^{-1} = \begin{pmatrix} 0 & I_n \\ -I_n & 0 \end{pmatrix}$$

Notice the immediate geometric consequence of skew-symmetry: the rate of change of $H$ along its own Hamiltonian flow is

$$\mathcal{L}_{X_H} H = dH(X_H) = (i_{X_H}\omega)(X_H) = \omega(X_H, X_H) = 0$$

Unlike gradient flow, which crosses level sets of $H$ orthogonally, **Hamiltonian flow is everywhere tangent to the iso-energy surfaces $H = \text{const}$**. Energy conservation is not a dynamical accident; it is the statement that a 2-form evaluated on a repeated vector vanishes.

#### Stream Functions on the Suminagashi Sheet (`libsumi`)

As we saw in Module 1 (§1.2), the two-dimensional canvas $M = \mathbb{R}^2$ with coordinates $(x, y)$ and area form $\omega = dx \wedge dy$ is itself a 1-DoF symplectic manifold with $q = x$ and $p = y$. Any scalar **stream function** $\psi(x, y)$ acts as a Hamiltonian on the tank surface:

$$i_{X_\psi}(dx \wedge dy) = d\psi \quad \Longrightarrow \quad X_\psi = \left( \frac{\partial \psi}{\partial y},\; -\frac{\partial \psi}{\partial x} \right), \qquad \nabla \cdot X_\psi = \frac{\partial^2 \psi}{\partial x \partial y} - \frac{\partial^2 \psi}{\partial y \partial x} = 0$$

Every incompressible velocity field on the sheet is locally a Hamiltonian vector field $X_\psi$. However, integrating $\dot{x} = X_\psi(x)$ for a finite step $\Delta t$ on a pixel grid requires care: evaluating a curved velocity field at a single point and taking a straight Euler step $P_{src} = P - \Delta t\,X_\psi(P)$ does **not** follow the curved streamlines of $\psi$, and its discrete Jacobian ceases to be unimodular ($\det J \neq 1$). `libsumi` handles this across its operator suite (`PROJECT_SPEC.md §10`, `PROJECT_SPEC.md §10.8.2`) in three distinct ways:

1. **Exact directional shear splitting (`chladni_fs`, §10.3):** The Chladni cellular operator is governed by the Taylor–Green vortex stream function:
   $$\psi(x, y) = \Psi \cos\!\big(k_x(x - x_0)\big)\cos\!\big(k_y(y - y_0)\big)$$
   Integrating $X_\psi$ directly by Euler would spiral outward from every cell centre. Instead, `chladni_fs` applies the trigonometric identity $\cos A \cos B = \frac{1}{2}[\cos(A - B) + \cos(A + B)]$ with $u = k_x(x - x_0)$ and $v = k_y(y - y_0)$ to split the Hamiltonian into two diagonal plane waves:
   $$\psi = \psi_- + \psi_+, \qquad \psi_\mp(u, v) = \frac{\Psi}{2}\cos(u \mp v)$$
   Because $\psi_-$ depends *only* on the combination $u - v$, its Hamiltonian vector field $X_{\psi_-}$ moves points purely along the lines $u - v = \text{const}$! Along each such line, $u - v$ is constant during the entire motion, so the velocity is constant along each trajectory and a single step is the **exact time-1 flow** of $\psi_-$ — a pure shear in $(u, v)$ with $\det J_- = 1$ identically. The second half-step along $u + v = \text{const}$ is an exact shear with $\det J_+ = 1$ reading the already-advected coordinate $(u_1, v_1)$:
   In the tree each wave is its own pass: `chladni_fs` applies ONE wave per pass, for any pair of wavevectors (a rectangular pair is Taylor–Green's, an oblique pair the Jankó's staggered rows), stage 0 then stage 1:
   ```glsl
   // deform.glsl, chladni_fs — one wave of ψ = ½Ψ[cos(w1·P') + cos(w2·P')]
   float s = wx * (P.x - p0x) + wy * (P.y - p0y);
   vec2 P_src = P + 0.5 * psi * weight * sin(s) * vec2(wy, -wx);
   ```
   The flow of $\frac{1}{2}\Psi\cos s$ is a pure shear along $(w_y, -w_x)$, the direction where $s$ is constant, so the lookup is exact at any amplitude; the second stage reads the field the first already advected.
   By the chain rule, $\det J = \det J_+ \cdot \det J_- = 1 \cdot 1 = 1$ for *any* amplitude `amp`. Moreover, because two shears along non-parallel axes do not commute, the exact inverse of a step is both shears negated in **reversed order** ($s_2$ first, then $s_1$) — a sign flip alone is not (`DECISIONS_5 #17, #23`); it is the conservation gate (`midi-sink --dev --soak chladni`) that holds the operator to its class, not a round-trip function.

2. **Strang-composed Lamb–Oseen rotations and Kirchhoff's point-vortex Hamiltonian (`SUMI_DEFORM_BRAID`, §10.8.2):** When a musical chord is held on the keyboard, each active note $k \in \{1, \dots, N\}$ spawns a regularized Lamb–Oseen vortex at $x_k = (x_k, y_k)$ with circulation $\Gamma_k = \operatorname{sign}(C_k)\,\Gamma_{\text{vel}}\,(0.3 + 0.7\,|C_k|)$ — $C_k$ the Sethares consonance of the note against the bass, $\Gamma_{\text{vel}} = 0.036$ canvas²/s at full velocity — and core radius $r_c$. The total stream function on the canvas is the superposition:
   $$\psi(x) = \sum_{k=1}^N \Gamma_k\,\psi_{\text{LO}}(\|x - x_k\|; r_c)$$
   Because each single vortex Hamiltonian $\psi_k(x) = \Gamma_k\,\psi_{\text{LO}}(\|x - x_k\|; r_c)$ is radially symmetric around $x_k$, its exact flow for any time step is a pure differential rotation (`vortex_fs`, $\det J_k = 1$ identically). Rather than summing the velocity fields on a grid and losing area preservation, `libsumi` integrates the superposed Hamiltonian $\psi = \sum_k \psi_k$ by **second-order Strang splitting**: a forward sweep $k = 1 \dots N$ with half-angle $\frac{1}{2}\Gamma_k \Delta t$ followed by a reverse sweep $k = N \dots 1$ with $\frac{1}{2}\Gamma_k \Delta t$ (30 exact rotation passes at the 15-voice cap). Every pass has $\det J = 1$, and the symmetric ordering cancels the $\mathcal{O}(\Delta t^2)$ commutator error $[X_{\psi_j}, X_{\psi_k}]$.

   Furthermore, in *free* mode (`braid_mode` = `SUMI_BRAID_FREE`, the D key; pinned is the default), the vortex centres $\{x_k = (X_k, Y_k)\}_{k=1}^N$ are not static: they mutually advect one another as a finite-dimensional Hamiltonian system of $N$ interacting point vortices! With the circulation-weighted symplectic form $\omega_{\text{vortex}} = \sum_{k=1}^N \Gamma_k\,dX_k \wedge dY_k$, the motion is governed by **Kirchhoff's regularized Hamiltonian**:
   $$H_{\text{Kirchhoff}}(x_1, \dots, x_N) = -\frac{1}{4\pi} \sum_{1 \le j < k \le N} \Gamma_j \Gamma_k \ln\!\left( \|x_j - x_k\|^2 + R_j R_k \right)$$
   (the regulariser is the product of the two cores, `rc2[j][k]`, not one $r_c^2$).
   So that the vortex centres never spiral apart during a ten-minute performance, `libsumi` steps $H_{\text{Kirchhoff}}$ on the CPU with the **implicit midpoint rule** (`sumi_braid_step`: resolved by fixed-point iteration in `double` to $10^{-15}$, at most sixteen rounds), which is a 2nd-order symplectic integrator on $(\mathbb{R}^{2N}, \omega_{\text{vortex}})$. In `tests/normalizer_tests.cpp`, a consonant triad braided for 72 000 steps at 1/120 s (ten minutes) holds $H_{\text{Kirchhoff}}$ within $|dH/H| < 10^{-6}$, and the linear impulse $\sum_k \Gamma_k x_k$ and the angular impulse $\sum_k \Gamma_k \|x_k\|^2$ within $10^{-9}$ (`sumi_braid_invariants`).

3. **First-order backward-characteristic steps and the folding horizon (`wake_fs`, `burst_fs`, §10.2):** When an operator does not split into closed-form shears or rotations — such as the potential dipole wake (`wake_fs`, `wake_profile` 0 — the default) or the viscous regularized Stokeslet wake (`wake_profile` 1) — `libsumi` takes a backward-characteristic step $P_{src} = P - u(P)$. For the potential doublet of radius $a$ and step length $d$, the velocity field is divergence-free ($\nabla \cdot u = 0$, so $\operatorname{tr}(\nabla u) = 0$), which kills the $\mathcal{O}(d/a)$ term in $\det(I - \nabla u) = 1 - \operatorname{tr}(\nabla u) + \det(\nabla u)$. However, the quadratic term $\det(\nabla u)$ survives! At the rear stagnation point $(r = a, \theta = \pi)$, the eigenvalues of $I - \nabla u$ are $1 - 2d/a$ and $1 + 2d/a$, giving:
   $$\det J_{\text{doublet}}\big|_{\text{rear}} = 1 - \frac{4d^2}{a^2} = 0 \quad \text{at } d = \frac{a}{2}$$
   At $d = a/2$, the map folds phase space onto a curve ($\det J = 0$); beyond $d > a/2$, $\det J < 0$ and the wake turns the ink inside out. The sub-step is the engine's — a stroke is cut into $n = \lceil \text{len}/(\text{tip}/4) \rceil$ steps of at most $a/4$, and the shader's comment says only "area-preserving to first order per sub-step, hence the ≤ a/4 budget (measured)"; the fold at $d = a/2$ from the rear stagnation point is this lesson's own derivation of why that budget is the right one. The Stokeslet is the viscous profile (`wake_profile` 1); the inviscid doublet (0) is the default.

---

### 2.2 Poisson Brackets, Lie Series & Non-Canonical Port-Hamiltonian Brackets

#### The Canonical Poisson Bracket

Given two smooth observables $f, g \in C^\infty(M)$ on a symplectic manifold $(M, \omega)$, each generates its own Hamiltonian vector field $X_f$ and $X_g$ via $i_{X_f}\omega = df$ and $i_{X_g}\omega = dg$. The **Poisson bracket** $\{f, g\}$ measures the rate of change of $f$ along the flow generated by $g$:

$$\{f, g\} := \omega(X_f, X_g) = (i_{X_f}\omega)(X_g) = df(X_g) = \mathcal{L}_{X_g} f$$

In canonical Darboux coordinates $(q, p)$, substituting $X_f = (\partial_p f, -\partial_q f)$ and $X_g = (\partial_p g, -\partial_q g)$ into $\omega = \sum_i dq_i \wedge dp_i$ yields:

$$\{f, g\} = \sum_{i=1}^n \left( \frac{\partial f}{\partial q_i}\frac{\partial g}{\partial p_i} - \frac{\partial f}{\partial p_i}\frac{\partial g}{\partial q_i} \right) = (\nabla f)^T J (\nabla g)$$

The time evolution of *any* observable $f(q, p)$ along the flow of a Hamiltonian $H$ is therefore governed by:

$$\frac{df}{dt} = \mathcal{L}_{X_H} f = \{f, H\} \qquad \left(\text{and } \frac{df}{dt} = \{f, H\} + \frac{\partial f}{\partial t} \text{ if } f \text{ depends explicitly on } t\right)$$

The Poisson bracket equips $C^\infty(M)$ with the structure of a **Lie algebra** over $\mathbb{R}$ and acts as a derivation in each slot (a **Poisson algebra**):
1. **Skew-symmetry:** $\{f, g\} = -\{g, f\}$ (since $\omega$ is alternating; in particular, $\{H, H\} = 0$, recovering energy conservation).
2. **Leibniz rule:** $\{f, gh\} = \{f, g\}h + g\{f, h\}$ (since $X_f$ is a first-order differential operator).
3. **Jacobi identity:** $\{\{f, g\}, h\} + \{\{g, h\}, f\} + \{\{h, f\}, g\} = 0$.

> **Why Module 1's $d\omega = 0$ is the Jacobi identity:** Using Cartan's identity on a 2-form, one can express the exterior derivative $d\omega(X_f, X_g, X_h)$ solely in terms of triple Poisson brackets:
> $$d\omega(X_f, X_g, X_h) = -2\Big( \{\{f, g\}, h\} + \{\{g, h\}, f\} + \{\{h, f\}, g\} \Big)$$
> Thus, the algebraic Jacobi identity for $\{ \cdot, \cdot \}$ is *equivalent* to the geometric closedness $d\omega = 0$! And as a corollary of the Jacobi identity, the map $f \mapsto X_f$ is a Lie-algebra homomorphism (up to sign convention) between functions under the Poisson bracket and vector fields under the Lie bracket: $[X_f, X_g] = -X_{\{f, g\}}$.

#### Formal Exponential Flows and Baker–Campbell–Hausdorff (BCH) Splitting

Because $\dot{f} = \{f, H\} = \mathcal{L}_{X_H} f$, the exact time-$h$ flow $\Phi_H^h : M \to M$ pulls observables back by the formal **Lie series**:

$$f \circ \Phi_H^h = \exp\!\left(h\,\mathcal{L}_{X_H}\right) f = f + h\{f, H\} + \frac{h^2}{2!}\{\{f, H\}, H\} + \frac{h^3}{3!}\{\{\{f, H\}, H\}, H\} + \cdots$$

For a general nonlinear Hamiltonian $H(q, p)$, we cannot evaluate $\exp(h\mathcal{L}_{X_H})$ in closed form. However, almost every mechanical and acoustic Hamiltonian in `midi-sink` is **separable**:

$$H(q, p) = T(p) + V(q)$$

Taken alone, the kinetic Hamiltonian $T(p)$ generates the vector field $X_T = (\nabla_p T(p),\, 0)$, where $p$ is constant during the entire step! Thus the exact time-$h$ flow of $T(p)$ is a trivial **drift** shear:

$$\Phi_T^h(q, p) = \big(q + h\,\nabla_p T(p),\; p\big), \qquad \det D\Phi_T^h = \det \begin{pmatrix} I & h\,\nabla_p^2 T(p) \\ 0 & I \end{pmatrix} = 1$$

Similarly, the potential Hamiltonian $V(q)$ generates $X_V = (0,\, -\nabla_q V(q))$, where $q$ is constant during the step. Its exact time-$h$ flow is a trivial **kick** shear:

$$\Phi_V^h(q, p) = \big(q,\; p - h\,\nabla_q V(q)\big), \qquad \det D\Phi_V^h = \det \begin{pmatrix} I & 0 \\ -h\,\nabla_q^2 V(q) & I \end{pmatrix} = 1$$

What happens when we compose these two exact flows? Because $\{T, V\} \neq 0$, the operators $\mathcal{L}_{X_T}$ and $\mathcal{L}_{X_V}$ do not commute. By the **Baker–Campbell–Hausdorff (BCH) formula**, the composition of two exponentials in a Lie algebra is the exponential of a single series of nested commutators (Poisson brackets):

$$\exp(h\mathcal{L}_{X_V}) \circ \exp(h\mathcal{L}_{X_T}) = \exp\!\big(h\,\mathcal{L}_{X_{\tilde{H}_{\text{SE}}}}\big), \qquad \tilde{H}_{\text{SE}} = T + V + \frac{h}{2}\{T, V\} + \frac{h^2}{12}\Big(\{T, \{T, V\}\} + \{V, \{V, T\}\}\Big) + \mathcal{O}(h^3)$$

This one-pass composition $\Phi_{\text{SE}}^h = \Phi_T^h \circ \Phi_V^h$ is **Symplectic Euler** (kick-then-drift). If instead we sandwich a full drift between two half-kicks — **Strang splitting**, known in mechanics as **2nd-order Störmer–Verlet / Leapfrog**:

$$\Phi_{\text{SV}}^h = \Phi_V^{h/2} \circ \Phi_T^h \circ \Phi_V^{h/2}$$

then the map is **time-reversible** ($\Phi_{\text{SV}}^{-h} = (\Phi_{\text{SV}}^h)^{-1}$), which forces every odd power of $h$ in the symmetric BCH expansion to vanish identically:

$$\tilde{H}_{\text{SV}} = T + V + \frac{h^2}{12}\{T, \{T, V\}\} - \frac{h^2}{24}\{V, \{V, T\}\} + \mathcal{O}(h^4)$$

Notice the miracle: **both $\Phi_{\text{SE}}^h$ and $\Phi_{\text{SV}}^h$ are exact compositions of canonical transformations** ($\det J = 1$ at every point in phase space), and for sufficiently small $h$, they are the exact time-$h$ flow of a nearby **shadow (modified) Hamiltonian** $\tilde{H}$.

#### Non-Canonical Brackets & Port-Hamiltonian Dirac Networks (`suzu::Dirac`, `voice_kind` 12)

Not every physical voice in Suzu is naturally written in canonical $(q, p)$ pairs. Consider `suzu::Dirac` (`voxo/src/suzu.h`, `DECISIONS_13 #3`), which models an interconnected acoustic network — specifically, a chain of masses coupled by springs to a loaded instrument **bridge** ($m_b, k_b$) with a unilateral **rattle stop** (like a sitar *jawari*, sawari, or clarinet lay) and a stick-slip bow port.

Instead of displacements $q_i$ and momenta $p_i$, `suzu::Dirac` chooses as its state vector $x \in \mathbb{R}^n$ the **spring extensions** $e_j = q_{j+1} - q_j$ ($j = 0 \dots m$) together with the **mass momenta** $p_i$ ($i = 1 \dots m$) and the bridge states $(e_b, p_b)$. Why? Because in extension-momentum variables, the total quadratic energy is purely **diagonal**:

$$H(x) = \frac{1}{2} x^T Q x = \sum_{j=0}^m \frac{1}{2} k\,e_j^2 + \sum_{i=1}^m \frac{1}{2} p_i^2 + \frac{1}{2} k_b\,e_b^2 + \frac{1}{2 m_b} p_b^2, \qquad Q = \operatorname{diag}(k, \dots, k, 1, \dots, 1, k_b, m_b^{-1}) \succ 0$$

There are $m + 1$ spring extensions for $m$ interior masses (fixed at both ends), so $\dim x = 2m + 3$ is **odd** (`Dirac::chain`: $m + 1$ extensions, $m$ momenta, the bridge's extension and momentum)! An odd-dimensional state space cannot carry a non-degenerate symplectic 2-form $\omega$. Instead, it carries a **non-canonical Poisson bracket** (and more generally, a **Dirac structure**) defined by the constant skew-symmetric incidence matrix $J = -J^T \in \mathbb{R}^{n \times n}$:

$$\{f, g\}_J := (\nabla f)^T J (\nabla g), \qquad J = \begin{pmatrix} 0 & B \\ -B^T & 0 \end{pmatrix}$$

Adding a symmetric positive semi-definite dissipation matrix $R = R^T \succeq 0$ (which damps the momenta to set the string's $T_{60}$ and the bridge's broader radiation loss $\gamma_b$) and external boundary ports $u_{\text{port}}$ gives the **Port-Hamiltonian system**:

$$\dot{x} = (J - R)\nabla H(x) + u_{\text{port}} = (J - R)Q x + u_{\text{port}}$$

Along any port-free trajectory ($u_{\text{port}} = 0$), the skew-symmetry of $J$ guarantees $x^T Q J Q x = 0$, so the energy balance is governed purely by $R$:

$$\frac{dH}{dt} = (\nabla H)^T \dot{x} = x^T Q (J - R) Q x = -x^T Q R Q x \le 0$$

How does `suzu::Dirac` integrate $\dot{x} = A x$ with $A = (J - R)Q$ so that this energy balance holds to machine precision over a ten-minute sustain? It applies the **Cayley transform** (the linear **implicit midpoint rule**):

$$\frac{x_{k+1} - x_k}{h} = A\,\frac{x_{k+1} + x_k}{2} \quad \Longleftrightarrow \quad x_{k+1} = M x_k, \qquad M = \left(I - \frac{h}{2}A\right)^{-1}\!\left(I + \frac{h}{2}A\right)$$

In `Dirac::build` (`voxo/src/suzu.h`), `M` is computed once at voice setup in `double` via Gauss–Jordan elimination with partial pivoting. Let us check the energy norm $\|x\|_Q^2 = x^T Q x = \|Q^{1/2} x\|_2^2$. In energy-normalized coordinates $y = Q^{1/2} x$, the generator becomes $\tilde{A} = Q^{1/2} A Q^{-1/2} = Q^{1/2}(J - R)Q^{1/2}$, whose symmetric part is $-Q^{1/2} R Q^{1/2} \preceq 0$. When $R = 0$, $\tilde{A} = Q^{1/2} J Q^{1/2}$ is purely skew-symmetric ($\tilde{A}^T = -\tilde{A}$), so the Cayley matrix $\tilde{M} = Q^{1/2} M Q^{-1/2} = (I - \frac{h}{2}\tilde{A})^{-1}(I + \frac{h}{2}\tilde{A})$ is **orthogonal**:

$$\tilde{M}^T \tilde{M} = I \quad \Longleftrightarrow \quad M^T Q M = Q \qquad (\text{$M$ is $Q$-orthogonal!})$$

> **Gate delta from `DECISIONS_13 #3`:** An early draft of the spec asked whether $M$ itself is orthogonal ($M^T M = I$). Unless $Q$ is a scalar multiple of the identity (equal spring constants and unit masses everywhere), $M$ is **$Q$-orthogonal** ($M^T Q M = Q$), meaning $\|Q^{1/2} M Q^{-1/2}\|_2 = 1$, *not* $\|M\|_2 = 1$. `Dirac::build` verifies this at runtime via 120 steps of power iteration on the Gram matrix of $Q^{1/2} M Q^{-1/2}$ (storing `gain_q`), and `tests/voxo_suzu_tests.cpp` (gate 59) checks twelve random skew matrices $J$ ($n$ from 6 to 16, at a step fifty times the engine's): $|\text{gain}_q - 1| < 10^{-9}$ when $R = 0$ — measured **`2e-16`** — and strictly `< 1` when $R \succ 0$. Notice also why `Dirac` stores `x` and `M` in `double`: a matrix that misses $Q$-orthogonality by `float`'s unit-in-the-last-place ($10^{-7}$) multiplies the energy by $(1 \pm 10^{-7})^N$ every sample — a runaway gain or loss over ten minutes at 192 kHz!

---

### 2.3 Liouville's Theorem, Symplectomorphisms & Ping-Pong Advection

#### Symplectomorphisms and Liouville's Theorem

A diffeomorphism $\Phi : M \to M$ is called a **symplectomorphism** (or **canonical transformation**) if it preserves the symplectic 2-form under pullback:

$$\Phi^*\omega = \omega \qquad \Longleftrightarrow \qquad (D\Phi_z)^T \Omega\,(D\Phi_z) = \Omega \quad \forall z \in M$$

When DOES a continuous flow $\Phi_X^t$ consist of symplectomorphisms? Taking the time derivative at $t = 0$ gives the Lie derivative $\mathcal{L}_X \omega$. By **Cartan's Magic Formula** from Module 1 (§1.3) and the closedness of $\omega$ ($d\omega = 0$):

$$\frac{d}{dt}\Big|_{t=0} (\Phi_X^t)^*\omega = \mathcal{L}_X\omega = d(i_X\omega) + i_X(\underbrace{d\omega}_{=\,0}) = d(i_X\omega)$$

For a Hamiltonian vector field $X_H$, we have $i_{X_H}\omega = dH$, and since $d^2 = 0$:

$$\mathcal{L}_{X_H}\omega = d(dH) = 0 \qquad \Longrightarrow \qquad (\Phi_H^t)^*\omega = \omega \quad \forall t \in \mathbb{R}$$

**Liouville's Theorem** is the immediate consequence: because $\Phi_H^t$ preserves $\omega$, it also preserves the $n$-fold wedge power $\Omega_{\text{vol}} = \frac{1}{n!}\omega^{\wedge n} = dq_1 \wedge dp_1 \wedge \cdots \wedge dq_n \wedge dp_n$ (the **Liouville phase-space volume form**):

$$\mathcal{L}_{X_H}\Omega_{\text{vol}} = 0, \qquad \det(D\Phi_H^t) = 1$$

In $2\text{D}$ ($n = 1$), $\Omega_{\text{vol}} = \omega = dq \wedge dp$, so **symplecticity and oriented area preservation are identical** ($\det J = 1$). (In dimension $2n \ge 4$, symplecticity $(D\Phi)^T \Omega (D\Phi) = \Omega$ is strictly stronger than volume preservation $\det D\Phi = 1$: it preserves the *sum* of the oriented area projections onto the conjugate $(q_i, p_i)$ planes — that much is linear algebra, $\omega$-preservation itself — and, far deeper, it forbids embedding a ball of radius $r$ into the cylinder $B^2(R) \times \mathbb{R}^{2n-2}$ for any $R < r$, which volume preservation alone would allow: **Gromov's non-squeezing theorem**.)

#### Why Standard Numerical Integrators Violate Liouville's Theorem

Suppose we integrate a 1-DoF separable Hamiltonian $H(q, p) = \frac{1}{2}p^2 + V(q)$ with equations of motion $\dot{q} = p$, $\dot{p} = -V'(q)$ across a finite time step $h$. Let us compute the Jacobian matrix $J = \frac{\partial(q_{n+1}, p_{n+1})}{\partial(q_n, p_n)}$ and its determinant $\det J$ for five standard schemes:

| Integrator | Update Rule $(q_n, p_n) \mapsto (q_{n+1}, p_{n+1})$ | Jacobian Determinant $\det J$ | Geometric Behaviour in Phase Space |
| :--- | :--- | :--- | :--- |
| **Forward Euler** | $q_{n+1} = q_n + h p_n$<br>$p_{n+1} = p_n - h V'(q_n)$ | $\det \begin{pmatrix} 1 & h \\ -h V''(q_n) & 1 \end{pmatrix} = \mathbf{1 + h^2 V''(q_n)}$ | **Artificial source** inside potential wells ($V'' > 0 \implies \det J > 1$). Phase-space area expands by $(1 + h^2 \omega_0^2)^N$; orbits spiral outward to $\infty$. |
| **Backward Euler** | $q_{n+1} = q_n + h p_{n+1}$<br>$p_{n+1} = p_n - h V'(q_{n+1})$ | $\det \begin{pmatrix} 1 & -h \\ h V''(q_{n+1}) & 1 \end{pmatrix}^{-1} = \mathbf{\frac{1}{1 + h^2 V''(q_{n+1})}}$ | **Artificial sink** inside potential wells ($V'' > 0 \implies \det J < 1$). Phase-space area contracts every step; orbits spiral inward to a dead point. |
| **Classical RK4** | 4-stage Runge–Kutta weighted average $z_{n+1} = z_n + \frac{h}{6}(k_1 + 2k_2 + 2k_3 + k_4)$ | For $V(q) = \frac{1}{2}\omega_0^2 q^2$ ($\varepsilon = \omega_0 h$):<br>$\mathbf{1 - \frac{1}{36}\varepsilon^6 + \frac{1}{576}\varepsilon^8 \neq 1}$ | **Higher-order non-symplectic drift.** Locally accurate to $\mathcal{O}(h^4)$, but $\det J \neq 1$! Over thousands of steps, area shrinks (or expands near hyperbolic points). |
| **Symplectic Euler** | $p_{n+1} = p_n - h V'(q_n)$<br>$q_{n+1} = q_n + h p_{n+1}$ | $\det \left[\begin{pmatrix} 1 & h \\ 0 & 1 \end{pmatrix} \begin{pmatrix} 1 & 0 \\ -h V''(q_n) & 1 \end{pmatrix}\right] = \mathbf{1}$ | **Exact symplectomorphism** (1st-order kick-then-drift). Preserves phase-space area to machine precision for *any* nonlinear $V(q)$ and any $h$. |
| **Störmer–Verlet (Strang Leapfrog)** | $p_{n+1/2} = p_n - \frac{h}{2} V'(q_n)$<br>$q_{n+1} = q_n + h p_{n+1/2}$<br>$p_{n+1} = p_{n+1/2} - \frac{h}{2} V'(q_{n+1})$ | $\det(J_{\text{kick}, h/2} \cdot J_{\text{drift}, h} \cdot J_{\text{kick}, h/2}) = 1 \cdot 1 \cdot 1 = \mathbf{1}$ | **Exact time-reversible symplectomorphism** (2nd-order). Preserves phase-space area identically and centers the shadow Hamiltonian to $\mathcal{O}(h^2)$. |

Look closely at the **Classical RK4** row. For a harmonic oscillator with dimensionless step $\varepsilon = \omega_0 h$, the single-step RK4 update matrix is $M_{\text{RK4}} = \left(1 - \frac{\varepsilon^2}{2} + \frac{\varepsilon^4}{24}\right)I + \left(\varepsilon - \frac{\varepsilon^3}{6}\right)J$, whose determinant is:

$$\det M_{\text{RK4}} = \left(1 - \frac{\varepsilon^2}{2} + \frac{\varepsilon^4}{24}\right)^2 + \left(\varepsilon - \frac{\varepsilon^3}{6}\right)^2 = 1 - \frac{\varepsilon^6}{72} + \frac{\varepsilon^8}{576}$$

For $\varepsilon < 2\sqrt{2}$, $\det M_{\text{RK4}} < 1$: **RK4 is slightly dissipative on elliptic orbits!** Over 10 steps, $\varepsilon^6/72$ is tiny; over $10^5$ audio samples or thousands of shader frames at moderate $h$, $(1 - \varepsilon^6/72)^N \to 0$. Even worse, on a **nonlinear pendulum** $H(q, p) = \frac{1}{2}p^2 - \cos q$, the local frequency $\omega_{\text{loc}}(q) = \sqrt{\cos q}$ varies across the phase portrait (and becomes imaginary near the hyperbolic saddles $q = \pm\pi$ where $\cos q < 0$), so $\det J_{\text{RK4}}(q, p)$ varies from pixel to pixel — compressing elliptic islands while stretching saddle regions!

#### Ping-Pong Advection, Phase-Space Stretching & Displacement Re-Basing (`libsumi`)

In Part 1 of this module's lab, we advect an initial circular phase-space distribution (a "Gibbs ensemble" blob of radius $R_0$ centered at $(q_0, p_0)$) under the **nonlinear pendulum Hamiltonian**:

$$H(q, p) = \frac{1}{2}p^2 - \cos(q), \qquad \dot{q} = p, \quad \dot{p} = -\sin(q)$$

Why does a circular blob in the pendulum's phase space stretch into a long, razor-thin spiral filament (a **whorl**)? Because the pendulum is **anharmonic**: orbits near the bottom of the well ($H \approx -1$) oscillate at period $T_0 = 2\pi$, whereas orbits approaching the **separatrix** ($H = +1$, the homoclinic orbit connecting the unstable saddle points $(q, p) = (\pm\pi, 0)$) slow down near the inverted top and have period $T(H) \to \infty$! Consequently, the outer edge of the blob lags behind the inner edge, shearing the disc into an ever-tighter spiral filament. Yet by **Liouville's Theorem**, as the filament stretches in length by a factor of $100\times$ and thins in width by $100\times$, **its total enclosed 2D area $\iint_{\text{blob}} dq\,dp$ must remain strictly constant**.

How do we advect a field across thousands of steps using two alternating WebGL framebuffers (**ping-pong FBOs**)? To update pixel $P = (q, p)$ from frame $n$ to frame $n+1$, a backward-characteristic shader traces $P$ **backward in time** by one step $-h$ using the inverse integrator $P_{src} = \Phi^{-h}(P)$ and samples the previous framebuffer at $P_{src}$. Here we hit the central architectural insight of `libsumi` (`PROJECT_SPEC.md §4.2`, `KESSHO §2.1`, the Kessho proposal `medium_specs_v3.md` (in git history; its substance §10.7)):

- **Why Direct Density Ping-Pong (`C'(P) = texture(C, P_src)`) blurs filaments:** If you store the dye density $C(q, p)$ directly in the ping-pong texture and resample `texture(u_prev, P_src)` with `GL_LINEAR` bilinear filtering every frame, each step applies a spatial averaging kernel of variance $\sim \mathcal{O}(\Delta x^2)$. Over $N$ frames, numerical diffusion grows as $\sqrt{N}\,\Delta x$. As soon as the pendulum shears the Liouville filament down to a few pixels in width, Eulerian bilinear resampling smears the filament away — destroying pixel area even when the underlying integrator has $\det J = 1$!
- **Why `libsumi` Advects Cumulative Displacement (`d'(P) = d(P_src) + (P_src - P)`):** Instead of resampling the pigment every frame, `libsumi`'s displacement field — the ping-pong texture, RGBA16F by default (RGBA32F behind a desktop flag) — stores the **total cumulative Lagrangian displacement vector** $d(P) = P_{\text{initial}}(P) - P$ pointing from the current pixel $P$ all the way back to its origin at $t = 0$. When an operator steps backward to $P_{src} = \Phi^{-h}(P)$, every operator pass (`deform.glsl`'s `*_fs`) updates the displacement vector by **re-basing**:
  ```glsl
  // libsumi displacement re-basing (PROJECT_SPEC.md §4.2):
  vec2 P_src = step_inverse(P, h);
  vec2 d_prev = texture(u_disp_prev, uv_from_phase(P_src)).xy;
  vec2 d_new  = d_prev + (P_src - P);   // total vector back to t = 0: P + d_new = P_src + d_prev = P_0
  ```
  Then, in the composite pass (`composite.glsl`), the initial blob $C_0(P_0)$ is evaluated **once** at $P_0 = P + d(P)$! Because $d(P)$ is a smooth mapping $P \mapsto P_0(P)$ whose Jacobian is $J_{\text{total}}(P) = I + \nabla d(P)$, a razor-thin filament remains infinitely crisp, and Kessho (`composite.glsl`'s `kessho_col`) reads the cumulative Jacobian by central differences over texels — the short stencil for $\psi$ and $\sigma$, ±3 texels at 512 for $\operatorname{tr} J$ — and its determinant $\det(I + \nabla d)$ in the `--kessho-flat` view at $z = 1$ to verify $|\det J - 1|$.

---

### 2.4 Symplectic Integrators at 48 kHz: Shadow Hamiltonians, Solitons & What Breaks Them

Now let us move from 60 Hz framebuffers to the audio thread running at **48 kHz** (and up to **192 kHz** with $m \le 8$ inner sub-steps) in `voxo/src/suzu.h`. Why does symplectic geometry matter even more for audio than for graphics? Because over a 10-minute ambient performance at 48 kHz, a single voice executes **$N = 28,800,000$ time steps**. Even a tiny per-step determinant defect $|\det J - 1| = 10^{-6}$ compounds to $(1 \pm 10^{-6})^{2.88 \times 10^7} \approx e^{\pm 28.8}$, either blowing the oscillator up — until its block is no longer finite and the bus breaker cuts it (the engine has no level threshold; the Suzu lab's worklet mutes at full scale and restarts) — or killing the note into silence!

#### Backward Error Analysis and the Shadow Hamiltonian of the Magic Circle

As we proved in Module 1 (§1.4), Suzu's core resonator `Cell::step` (`voxo/src/suzu.h`) is the **magic circle** (symplectic Euler / kick-drift on $H(x, y) = \frac{1}{2}(x^2 + y^2)$ with $\varepsilon = 2\sin(\omega_0 \Delta t / 2)$):

$$x_{n+1} = x_n - \varepsilon y_n, \qquad y_{n+1} = y_n + \varepsilon x_{n+1}, \qquad M_{\text{sym}} = \begin{pmatrix} 1 & -\varepsilon \\ \varepsilon & 1 - \varepsilon^2 \end{pmatrix}, \quad \det M_{\text{sym}} = 1$$

If you compute the **true continuous Hamiltonian** $H(x_n, y_n) = \frac{1}{2}(x_n^2 + y_n^2)$ along a trajectory of `Cell::step`, you will notice that $H(x_n, y_n)$ is **not** constant from sample to sample! Specifically:

$$H(x_{n+1}, y_{n+1}) - H(x_n, y_n) = \frac{\varepsilon^2}{2}\big(x_{n+1}^2 - y_n^2\big) = \mathcal{O}(\varepsilon)$$

Does this mean the magic circle leaks or gains energy? **No!** As shown by backward error analysis (and verified algebraically in Module 1 Problem 1.3), `Cell::step` exactly conserves a nearby quadratic form — its **shadow (modified) Hamiltonian**:

$$\tilde{H}_{\text{MC}}(x, y) = \frac{1}{2}\big(x^2 + y^2 - \varepsilon x y\big) \qquad \Longrightarrow \qquad \tilde{H}_{\text{MC}}(x_{n+1}, y_{n+1}) = \tilde{H}_{\text{MC}}(x_n, y_n) \quad \forall n \ge 0$$

For $|\varepsilon| < 2$, the level sets of $\tilde{H}_{\text{MC}}$ are tilted ellipses in the $(x, y)$ plane with semi-axis ratio $\sqrt{(2+\varepsilon)/(2-\varepsilon)} \approx 1 + \varepsilon/2$. Because the discrete orbit walks faithfully around this fixed shadow ellipse forever, the continuous circular energy $H(x_n, y_n) = \frac{1}{2}(x_n^2 + y_n^2)$ simply **oscillates ("breathes")** at twice the oscillator frequency ($2\omega_0$) within a tight band of width $\pm \varepsilon/2$ around $\tilde{H}_{\text{MC}}$, with **zero secular drift** ($+0.0001\text{ dB}$ over 600 s at 440 Hz in `suzu_test.cpp`).

If instead we use the symmetric **2nd-order Störmer–Verlet (Leapfrog)** half-kick / drift / half-kick:

$$y_{n+1/2} = y_n + \frac{\varepsilon}{2}x_n, \qquad x_{n+1} = x_n - \varepsilon y_{n+1/2}, \qquad y_{n+1} = y_{n+1/2} + \frac{\varepsilon}{2}x_{n+1}$$

the tilted cross-term $-\varepsilon x y$ cancels out completely, and the exact shadow Hamiltonian becomes axis-aligned:

$$\tilde{H}_{\text{SV}}(x, y) = \frac{1}{2}\!\left(\left(1 - \frac{\varepsilon^2}{4}\right)x^2 + y^2\right)$$

so the breathing of the true energy $H(x_n, y_n)$ shrinks from $\mathcal{O}(\varepsilon)$ to $\mathcal{O}(\varepsilon^2)$!

This exact mechanism carries over to **nonlinear** separable Hamiltonians, where no closed-form quadratic invariant exists, via the BCH asymptotic shadow series $\tilde{H} = H + h^2 H_2 + h^4 H_4 + \cdots$:
- **The Duffing oscillator (`suzu::Duffing`, `voxo/src/suzu.h`):** Adding the cubic restoring force to the kick shear (`c.y += c.eps * (x + beta * x * x * x)`) integrates $H(x, y) = \frac{1}{2}y^2 + \frac{1}{2}x^2 + \frac{\beta}{4}x^4$. Even though $x^3$ is strongly nonlinear (bending the pitch upward with amplitude for $\beta > 0$), the kick depends only on the already-updated $x_{n+1}$, so its Jacobian is still unit lower-triangular with $\det J = 1$ identically.

#### The Toda Lattice Soliton Ring (`suzu::TodaRing`, `voice_kind` 10)

The crown jewel of nonlinear symplectic integration in `midi-sink` is the **Toda Lattice Soliton Ring** (`suzu::TodaRing`, `voxo/src/suzu.h`; `PROJECT_SPEC.md §13.13`; `DECISIONS_13 #1`; [the Toda ring chapter](../../suzu/toda-ring/)). Morikazu Toda discovered in 1967 that a 1D ring of $N$ unit masses interacting via an **exponential nearest-neighbour spring** is **completely integrable** (possessing $N$ independent Lax-pair invariants in involution) and supports exact supersonic **cnoidal waves and solitons**:

$$H(q, p) = \sum_{n=0}^{N-1} \frac{1}{2}p_n^2 + \frac{a}{b}\sum_{n=0}^{N-1} \left( e^{-b r_n} + b r_n - 1 \right), \qquad r_n = q_{(n+1)\bmod N} - q_n, \quad k = a b$$

Notice that as the nonlinearity parameter $b \to 0$, Taylor-expanding $e^{-b r_n} \approx 1 - b r_n + \frac{1}{2}b^2 r_n^2$ reduces $H$ to the linear harmonic mass-spring ring $H \to \sum_n \big(\frac{1}{2}p_n^2 + \frac{1}{2}k r_n^2\big)$. For $b > 0$, compression ($r_n < 0$) stiffens exponentially while extension ($r_n > 0$) softens, allowing localized compression pulses $\operatorname{sech}^2(\kappa(n - ct))$ to travel around the ring faster than the sound speed $\sqrt{k}$ and pass right through one another without losing their shape!

Let us examine `TodaRing::step` (`voxo/src/suzu.h`):

```cpp
// From voxo/src/suzu.h — TodaRing::step (Voxo 0.23.0; checked word for word at the review):
inline void step(float damp) {                           // the kick from the pre-update q, then the drift; damp the declared contraction on p
    double E[TODA_NODES_MAX];
    double x[TODA_NODES_MAX];
    for (int i = 0; i < n; i++) x[i] = -b * r_at(i);
    for (int i = 0; i < n; i++) E[i] = toda_exp(x[i]);
    for (int i = 0; i < n; i++) if (E[i] > 0.5 * kcap) E[i] = kcap - 0.25 * kcap * kcap / E[i];
    double sum_p = 0.0;
    for (int i = 0; i < n; i++) { const double f = a * (E[i > 0 ? i - 1 : n - 1] - E[i]); p[i] = (p[i] + f) * (double)damp; sum_p += p[i]; }
    for (int i = 0; i < n; i++) q[i] += p[i];
    pick_xi -= sum_p / (double)n;                        // the pickup stands still against the mean drift
    if (pick_xi >= (double)n) pick_xi -= (double)n; else if (pick_xi < 0.0) pick_xi += (double)n;
}
```

Every line of `TodaRing` teaches a concrete lesson about symplectic integration at audio rate:
1. **Why `double` state (`q`, `p`) is mandatory:** With `damp = 1.0f`, the kick-then-drift leapfrog is an exact symplectomorphism ($\det J = 1$) in exact arithmetic. However, at 192 kHz ($1.15 \times 10^8$ steps over 10 minutes), single-precision `float` rounding of `q[i] += p[i]` alone behaves like a tiny random non-symplectic perturbation, walking the energy by **`0.009 dB/min`** (failing the `0.001 dB` 10-minute gate). Promoting the ring's state to `double` drops the 10-minute minute-mean drift to **`0.000017 dB`** (`0.000004 dB` re-measured with `suzu::toda_exp`, a degree-11 Taylor after Cody–Waite range reduction, $10^{-14}$ relative — the same bits on every platform), while the instantaneous continuous energy $H(q, p)$ breathes by **`±0.06 dB`** around the shadow Hamiltonian $\tilde{H}$! Over 1,000 ring transits, the soliton's amplitude holds within **`0.26 %`**, its width×amplitude product within **`0.72 %`**, and total ring momentum $\sum_i p_i$ (the Noether invariant of translation around the ring) to **`1e-14`**.
2. **Why the stiffness cap must be $C^1$ (`E = kcap - kcap²/(4E)`):** Under a massive MIDI strike or multi-soliton collision, $e^{-b r_n}$ could spike past the leapfrog's stability ceiling. If you clamp $e^{-b r_n}$ with a hard `std::min(E, kcap)`, the potential $V(r)$ has a $C^1$ derivative kink ($V''(r)$ jumps discontinuously to zero at the knee). Whenever a soliton crosses a non-smooth $C^1$ kink, the BCH shadow Hamiltonian expansion breaks down and the leapfrog feeds energy on the corner! Replacing `min` with the smooth rational saturation $E_{\text{cap}}(E) = E$ for $E \le \frac{1}{2}k_{\text{cap}}$ and $k_{\text{cap}} - \frac{k_{\text{cap}}^2}{4E}$ above it makes $V(r)$ $C^2$-smooth across the knee $r_c = -\frac{1}{b}\ln(\frac{1}{2}k_{\text{cap}})$.

#### What Can Break a Symplectic Integrator? (Four Engineering Failure Modes)

Because a symplectic integrator has $\det J = 1$, students sometimes assume it is unconditionally stable and can never blow up. That is **false**. Here are the four ways a symplectic integrator can fail, and how `midi-sink` prevents each one:

1. **Non-Separable Hamiltonians (`suzu::DoublePendulum`, `voxo/src/suzu.h`):**
   Consider the chaotic double pendulum modulator (§13.14), whose kinetic energy $T(\theta_1, \theta_2, \omega_1, \omega_2) = \omega_1^2 + \frac{1}{2}\omega_2^2 + \omega_1 \omega_2 \cos(\theta_1 - \theta_2)$ depends on *both* the angles $\theta$ and the angular velocities $\omega$ through the coupling $\cos(\theta_1 - \theta_2)$. Because $H$ is **not separable** ($H(\theta, p) \neq T(p) + V(\theta)$), the "drift" step $\dot{\theta} = \partial_p H(\theta, p)$ depends on $\theta$, and the "kick" step $\dot{p} = -\partial_\theta H(\theta, p)$ depends on $p$! If you apply an explicit leapfrog to a non-separable Hamiltonian, the individual sub-steps are no longer triangular shears ($\det J \neq 1$), the scheme is **not symplectic**, and as documented in `voxo/src/suzu.h`, *"the first probe drifted from $E = -2$ to $+1.4$ in ten minutes (and to NaN from a hard kick)."*
   Because a true symplectic integrator for a non-separable Hamiltonian requires an implicit solve at every sub-step, `DoublePendulum` instead belongs to Suzu's declared **Projected** class: it steps in `double` using classical 4th-order Runge–Kutta (`substep()`) and then projects the state back onto the initial strike energy surface $H = E_{\text{set}}$ via a velocity rescale $r = \sqrt{(E_{\text{set}} - V)/T}$ (a single-step projection factor within $9 \times 10^{-8}$ of $1.0$).

2. **Time-Dependent Potentials & Live-Peak Limiter Pumping (`DECISIONS_13 #1`):**
   Remember from §2.2 that along a Hamiltonian trajectory, $\frac{dH}{dt} = \{H, H\} + \frac{\partial H}{\partial t} = \frac{\partial H}{\partial t}$. If a programmer makes a potential parameter depend on time during the sustain — for instance, adjusting a stiffness limiter or soft-cap threshold $k_{\text{cap}}(t)$ dynamically on every audio block based on the ring's current peak compression — then the Hamiltonian becomes explicitly time-dependent: $H = H(q, p, t)$ with $\frac{\partial H}{\partial t} \neq 0$! As found during step 83 (`DECISIONS_13 #1`, item 3 — a development finding, never a shipped control; this module's standalone lab offers it as its own red control), modulating the Toda cap from the live peak compression acts like a child pumping a swing: the time-varying potential does net positive work on each soliton transit, **pumping `+162 dB/s`** into the ring. The ring's shipped red control is a different one: `toda_gate` 0 lifts the resolution rule, and the energy climbs +19 dB in a second at C8 (gate 52). That is why `TodaRing` fixes its resolution sub-stepping $m \in \{2, 4, 6, 8\}$ (to satisfy the **Resolution Rule** $s = \sqrt{k}\cdot\hat{c}\cdot b \le 0.15$) and its $C^1$ cap `kcap` **once at the strike**, keeping $\frac{\partial H}{\partial t} = 0$ throughout the sustain.

3. **Exceeding the Linear Stability Horizon ($|\operatorname{tr} J| > 2$ / CFL Violation):**
   A $2 \times 2$ unimodular matrix ($\det J = 1$) has characteristic polynomial $\lambda^2 - (\operatorname{tr} J)\lambda + 1 = 0$.
   - When $|\operatorname{tr} J| < 2$, the eigenvalues $\lambda_\pm = e^{\pm i\theta}$ lie on the **unit circle** (elliptic stability; shadow Hamiltonian is an ellipse).
   - When $|\operatorname{tr} J| > 2$, the eigenvalues $\lambda_+ > 1$ and $\lambda_- = 1/\lambda_+ < 1$ are **real and reciprocal**! Even though $\det J = \lambda_+ \lambda_- = 1$ (area is still preserved: one axis stretches while the other shrinks!), the shadow Hamiltonian turns into a **hyperbola** and the state explodes exponentially along the unstable eigenvector as $\lambda_+^n$.
   For the magic circle / 1D leapfrog, $\operatorname{tr} M_{\text{sym}} = 2 - \varepsilon^2$, so stability requires $\varepsilon < 2$ ($\omega_0 \Delta t < \pi$, the Nyquist limit; Suzu's bounds are each voice's own: the chain's Courant number $s \le 1$ (`string_cfl`; 1.05 is the red control), the ring's $k \cdot \text{cap} \le 0.9$ at the note together with the resolution rule $\sqrt{k}\,\hat{c}\,b \le 0.15$, and $f < 1.9$ the SVF's).

4. **Nonlinear Parametric (Mathieu) Resonance on the 2D Gong (`suzu::Membrane`, `voice_kind` 9, §13.9):**
   Even below the linear Courant bound $\lambda_{2D} < 2$, a nonlinear symplectic system can suffer **parametric resonance**. On Suzu's 2D Rectangular Membrane (`voxo/src/suzu.h`), the linear leapfrog stability bound is $\lambda_{2D} = c\,\Delta t \sqrt{dx^{-2} + dy^{-2}} \le 1.95$. However, when the **von Kármán gong nonlinearity** ($\alpha > 0$) is enabled, the global membrane tension $c^2 + \alpha E$ (where $E = \sum |\nabla_h u|^2 = -\sum u \Delta_h u$) is modulated by the fundamental $(1, 1)$ mode's oscillation at $2\omega_{11}$. Even though the gong's kick-drift step is an exact momentum shear with $\det J = 1$ in $2 n_x n_y$-dimensional phase space, in this lesson's reading this periodic tension modulation turns the highest grid mode's equation of motion into the **Mathieu equation**: when the top mode sits near the $\operatorname{tr} J = -2$ stability boundary ($\lambda_{2D} \in [1.82, 1.95]$), its conjugate unit-circle eigenvalues collide at $\lambda = -1$ and split onto the real axis ($\det J = 1$ still holds while the top mode's energy grows without bound). The record says only what was measured — a swelling sheet at 1.95 diverged — and the gong's bar is **`1.80`** (`MEMBRANE_CFL_GATE_GONG`), which in this reading keeps the top mode's Floquet multiplier away from $-1$; the ten-minute energy drift measured inside **`0.0002 dB`** ($-0.00016$ dB on the 8 × 6 grid at 96 kHz).

---

## Lab Instructions (`module_02_lab.html`)

Open [the module's lab](../module_02_lab.html) (a standalone page — a teaching re-implementation, never the engine) in a WebGL2 + WebAudio browser. The interactive lab is divided into two synchronized workspaces matching the lecture notes.

### Part 1: Shader Lab — Ping-Pong Phase-Space Advection & Area Preservation

The top workspace runs a real-time **WebGL2 `RGBA32F` Ping-Pong Framebuffer Pipeline** on the phase space $(q, p) \in [-\pi, \pi]^2$ of the **nonlinear pendulum** $H(q, p) = \frac{1}{2}p^2 - \cos(q)$:

1. **Compare Symplectic Störmer–Verlet vs. Classical RK4 vs. Euler:**
   - Start with the default **2nd-Order Symplectic Störmer–Verlet (Leapfrog)** integrator and click **Reset Phase Blob**. Watch the circular phase-space ensemble (placed across the anharmonic shear zone near the separatrix $H = +1$) stretch into a long, razor-thin spiral whorl over thousands of frames.
   - Observe the live GPU readback telemetry in the top bar: **Blob Area Ratio $A(t)/A_0$** stays locked at `100.0 %`, and the **`--kessho-flat` Gate $\max|\det J - 1|$** remains near `0.000` (up to discrete pixel-grid finite differences along the whorl edge).
   - Now switch the **Integrator Scheme** to **4th-Order Runge–Kutta (RK4)** and increase the step size $h$ (or sub-steps/frame). Even though RK4 is fourth-order accurate per step, watch $\det J_{\text{RK4}} \neq 1$ steadily shrink (or distort) the total pixel area of the blob over thousands of frames!
   - Switch to **Forward Euler ($\det J = 1 + h^2\cos q > 1$)** or **Backward Euler ($\det J < 1$)** — the red controls — to see immediate catastrophic phase-space area creation (the blob balloons out of the well) or artificial dissipation (the blob collapses into the origin $(0,0)$).

2. **Test `libsumi` Displacement Re-Basing vs. Naive Density Ping-Pong:**
   - Toggle the **Advection Architecture** selector between:
     - **`libsumi` Displacement Re-Basing (`d'(P) = d(P_src) + (P_src - P)`)**: The ping-pong FBO advects the 2D Lagrangian origin offset `RGBA32F`, and the composite pass evaluates the sharp initial blob $C_0(P + d(P))$ once. Notice how the stretched filament stays razor-sharp even after 5,000 steps.
     - **Direct Density Ping-Pong (`C'(P) = texture(C, P_src)`)**: The ping-pong FBO resamples the scalar pigment density every frame using bilinear filtering. Watch how numerical diffusion smears the thin filament into a blurry cloud and erodes the thresholded pixel area — proving why `libsumi` stores cumulative displacements in Layer A!

3. **Inspect with Kessho Birefringence & the $z = 1$ `--kessho-flat` Gate:**
   - Switch the **Display Mode** between **Phase-Space Filament + $H(q,p)$ Contours**, **Kessho Birefringence & Caustics ($\det J$)**, and **`--kessho-flat` ($z=1$) $|\det J - 1|$ Gate** (where green means $|\det J - 1| \le 0.05$ and red flags area compression/expansion).
   - Click anywhere on the phase canvas to drop the ensemble at a new initial $(q_0, p_0)$ — try placing it right on the hyperbolic saddle $(q, p) = (\pi, 0)$ or astride the separatrix!

### Part 2: Sound Lab — Audifying Symplectic Shadow Hamiltonians at 48 kHz

The bottom workspace runs a **48 kHz WebAudio synthesizer** integrating a fast physical oscillator alongside a **Reference Energy-Pitch Drone** whose frequency is exponentially mapped to the instantaneous energy ratio:

$$f_{\text{drone}}(t) = f_{\text{ref}} \cdot 2^{\,\gamma \, \frac{H(q(t),\,p(t)) - H_0}{H_0}} \qquad \left(\text{or mapped to the exact shadow Hamiltonian } \tilde{H}(q, p)\right)$$

When $H(q(t), p(t)) = H_0$, the reference drone forms a pure **unison** (or exact octave) with the oscillator:

1. **Hear the Integrators by Ear:**
   - Click **Start 48 kHz Audio** and select **Symplectic Störmer–Verlet (Leapfrog)** or **Symplectic Euler (Magic Circle)**. The pitch of the reference drone locks indefinitely!
   - Toggle the **Drone Pitch Mapping** between **True Continuous Energy $H(q, p)$** and **Shadow Hamiltonian $\tilde{H}(q, p)$** while pushing the dimensionless step $\varepsilon = \omega_0 \Delta t$ up toward `0.8–1.4`. On $H(q, p)$, you will see (on the live Energy Strip Chart) and hear (as a subtle FM shimmer at $2\omega_0$) the bounded $\mathcal{O}(\varepsilon)$ or $\mathcal{O}(\varepsilon^2)$ **shadow breathing** of $H$ around $\tilde{H}$, with **zero secular pitch drift**. Switch the mapping to $\tilde{H}(q, p)$ and the FM breathing vanishes completely — the line is flat to machine precision!
   - Click **Forward Euler (Red Control)**: Because $\det J = 1 + \varepsilon^2 > 1$, the energy grows exponentially every sample ($H_n = (1 + \varepsilon^2)^n H_0$), producing an unmistakable **upward pitch glissando** on the reference drone until the lab's safety breaker clamps the voice (the lab's own, at +14 dB over the strike's energy; the engine's bus breaker has no level threshold — it cuts a block that is no longer finite)!
   - Click **Backward Euler (Red Control)**: Because $\det J = (1 + \varepsilon^2)^{-1} < 1$, the energy decays exponentially, pulling the reference drone in a **downward pitch spiral** while the oscillator fades into silence.
   - Click **Classical RK4**: Push $\varepsilon$ to `0.9–1.3`. You will hear RK4 hold pitch briefly and then sag flat in a slow downward glissando as $(1 - \varepsilon^6/72)^N$ drains the phase-space area over hundreds of thousands of audio samples!

2. **Switch Nonlinear Potentials & Trigger the Live-Cap Pumping Red Control:**
   - Switch the **Oscillator Hamiltonian** between **Harmonic ($V = \frac{1}{2}q^2$)**, **Duffing Hardening Spring ($V = \frac{1}{2}q^2 + \frac{\beta}{4}q^4$)**, and **2-Body Toda Soliton Bond ($V = \frac{a}{b}(e^{-bq} + bq - 1)$)**.
   - Enable the **Time-Dependent Live Cap (`∂H/∂t ≠ 0` Red Control)**: This modulates the stiffness cap dynamically from the oscillator's live peak every block (the failure mode found at step 83, `DECISIONS_13 #1` — the lab's own control; the engine never shipped one). Even under Symplectic Leapfrog, watch and hear the energy ratchet upward step after step as $\frac{dH}{dt} = \frac{\partial H}{\partial t} \neq 0$ pumps work into the orbit!

---

## Problem Set

### Problem 2.1: RK4 Determinant Defect & The Störmer–Verlet Shadow Hamiltonian

Consider the 1D harmonic oscillator $H(q, p) = \frac{1}{2}\omega_0(q^2 + p^2)$ integrated with dimensionless time step $\varepsilon = \omega_0 h$.

1. **Classical RK4 is not symplectic:**
   For the linear system $\dot{z} = \varepsilon J z$ with $J = \begin{pmatrix} 0 & 1 \\ -1 & 0 \end{pmatrix}$ and $J^2 = -I$, the 4-stage Runge–Kutta update matrix is the truncated Taylor series of $\exp(\varepsilon J)$:
   $$M_{\text{RK4}}(\varepsilon) = I + \varepsilon J + \frac{\varepsilon^2}{2!}J^2 + \frac{\varepsilon^3}{3!}J^3 + \frac{\varepsilon^4}{4!}J^4 = \left(1 - \frac{\varepsilon^2}{2} + \frac{\varepsilon^4}{24}\right)I + \left(\varepsilon - \frac{\varepsilon^3}{6}\right)J$$
   Compute $\det M_{\text{RK4}}(\varepsilon)$ explicitly as a polynomial in $\varepsilon$ and verify that:
   $$\det M_{\text{RK4}}(\varepsilon) = 1 - \frac{\varepsilon^6}{72} + \frac{\varepsilon^8}{576}$$
   At $f_0 = 2000\text{ Hz}$ and $f_s = 48\text{ kHz}$ ($\varepsilon = 2\pi f_0 / f_s \approx 0.2618$), by how many decibels ($10\log_{10}(\det M_{\text{RK4}})^N$) does RK4's energy drift over a 10-minute sustain ($N = 2.88 \times 10^7$ samples)? Compare this against Suzu's `0.001 dB` gate.

2. **Exact Shadow Hamiltonian of 2nd-Order Störmer–Verlet:**
   Write the single-step matrix $M_{\text{SV}}(\varepsilon)$ of the symmetric half-kick / drift / half-kick leapfrog:
   $$M_{\text{SV}}(\varepsilon) = \begin{pmatrix} 1 & 0 \\ -\varepsilon/2 & 1 \end{pmatrix} \begin{pmatrix} 1 & \varepsilon \\ 0 & 1 \end{pmatrix} \begin{pmatrix} 1 & 0 \\ -\varepsilon/2 & 1 \end{pmatrix}$$
   Multiply the three matrices out, verify $\det M_{\text{SV}}(\varepsilon) = 1$ for all $\varepsilon \in \mathbb{R}$, and prove that the quadratic form
   $$\tilde{H}_{\text{SV}}(q, p) = \frac{1}{2}\!\left(1 - \frac{\varepsilon^2}{4}\right)q^2 + \frac{1}{2}p^2$$
   is conserved **identically** ($\tilde{H}_{\text{SV}}(q_{n+1}, p_{n+1}) = \tilde{H}_{\text{SV}}(q_n, p_n)$) along every discrete trajectory whenever $|\varepsilon| < 2$. Why does $\tilde{H}_{\text{SV}}$ have no $qp$ cross-term unlike the magic circle's $I_\varepsilon(q, p) = \frac{1}{2}(q^2 + p^2 - \varepsilon q p)$, and why does the true energy $H(q_n, p_n)$ breathe by $\mathcal{O}(\varepsilon^2)$ instead of $\mathcal{O}(\varepsilon)$?

---

### Problem 2.2: Toda Lattice Solitons, The $C^1$ Cap & Time-Dependent Pumping (`suzu::TodaRing`)

In `voxo/src/suzu.h`, `suzu::TodaRing` integrates the $N$-node exponential lattice with bond strain $r_i = q_{(i+1)\bmod N} - q_i$:

$$H(q, p) = \sum_{i=0}^{N-1} \frac{1}{2}p_i^2 + \sum_{i=0}^{N-1} V(r_i), \qquad V(r) = \frac{a}{b}\left(e^{-br} + br - 1\right), \quad k = ab$$

1. **Noether Momentum Conservation & Symplecticity:**
   Prove that the kick-then-drift update in `TodaRing::step(1.0f)`:
   $$p_i^{n+1} = p_i^n + a\left(E_{i-1}(q^n) - E_i(q^n)\right), \qquad q_i^{n+1} = q_i^n + p_i^{n+1}$$
   has unit Jacobian determinant $\det \frac{\partial(q^{n+1}, p^{n+1})}{\partial(q^n, p^n)} = 1$ on $\mathbb{R}^{2N}$ for *any* bond force law $E_i(q^n)$, and preserves the total ring momentum $P_{\text{tot}} = \sum_{i=0}^{N-1} p_i$ identically at every step. Explain why `TodaRing` must subtract the mean drift $\bar{p} = P_{\text{tot}} / N$ from the pickup coordinate (`pick_xi -= sum_p / n`) when the swirl impulse (`kick_all`) is non-zero.

2. **Continuity of the Rational Stiffness Cap:**
   To prevent extreme compressions ($r < r_c$) from violating the leapfrog stability ceiling, `TodaRing::step` replaces $E(r) = e^{-br}$ past the knee $E(r_c) = \frac{1}{2}k_{\text{cap}}$ (so $r_c = -\frac{1}{b}\ln(\frac{1}{2}k_{\text{cap}})$) with:
   $$E_{\text{sat}}(r) = \begin{cases} e^{-br} & r \ge r_c \\ k_{\text{cap}} - \dfrac{k_{\text{cap}}^2}{4 e^{-br}} & r < r_c \end{cases}$$
   Show that both $E_{\text{sat}}(r)$ and its derivative $E_{\text{sat}}'(r)$ are continuous at $r = r_c$, so the corresponding capped potential $V_{\text{cap}}(r)$ in `TodaRing::energy()` is **$C^2$-smooth**, and its local stiffness $-V_{\text{cap}}''(r) = a\,E_{\text{sat}}'(r)$ attains a maximum of $\frac{1}{2}k\,k_{\text{cap}}$ right at the knee $r = r_c$.

3. **Why Live-Peak Limiting Breaks Energy Conservation:**
   Suppose that instead of fixing $k_{\text{cap}}$ at the strike, we set $k_{\text{cap}}(t)$ dynamically on every audio block as a function of the instantaneous peak compression. Using $\frac{dH}{dt} = \{H, H\} + \frac{\partial H}{\partial t}$, compute $\frac{\partial V_{\text{cap}}}{\partial k_{\text{cap}}}\,\dot{k}_{\text{cap}}$ for a bond in the capped regime ($r < r_c$) and explain why a time-varying cap acts as a parametric energy pump even when the single-step map has $\det J = 1$.

---

### Problem 2.3: $Q$-Orthogonality of the Cayley Transform in `suzu::Dirac`

In `voxo/src/suzu.h`, `suzu::Dirac` integrates the port-Hamiltonian network $\dot{x} = (J - R)\nabla H(x)$ with quadratic energy $H(x) = \frac{1}{2}x^T Q x$ ($Q = Q^T \succ 0$, $J^T = -J$, $R = R^T \succeq 0$) using the Cayley / implicit-midpoint matrix:

$$M = \left(I - \frac{h}{2}A\right)^{-1}\!\left(I + \frac{h}{2}A\right), \qquad A = (J - R)Q$$

1. **Proof of Passivity and $Q$-Orthogonality:**
   For any trajectory $x_{k+1} = M x_k$, define the midpoint vector $z = \frac{1}{2}(x_{k+1} + x_k) = \left(I - \frac{h}{2}A\right)^{-1} x_k$, so that $x_k = z - \frac{h}{2}Az$ and $x_{k+1} = z + \frac{h}{2}Az$. Expand the one-step energy change $H(x_{k+1}) - H(x_k) = \frac{1}{2}x_{k+1}^T Q x_{k+1} - \frac{1}{2}x_k^T Q x_k$ in terms of $z$ and prove the exact identity:
   $$H(x_{k+1}) - H(x_k) = h\,z^T Q A z = h\,z^T Q(J - R)Q z = -h\,z^T Q R Q z \le 0$$
   Conclude that when $R = 0$, $M^T Q M = Q$ ($M$ is $Q$-orthogonal and preserves $H(x)$ for *every* $h > 0$), and when $R \succeq 0$, $\|Q^{1/2} M Q^{-1/2}\|_2 \le 1$ (strictly passive for all $h > 0$).

2. **Why `M` is $Q$-Orthogonal, Not Orthogonal:**
   Give an explicit $2 \times 2$ example with $J = \begin{pmatrix} 0 & 1 \\ -1 & 0 \end{pmatrix}$, $R = 0$, and $Q = \operatorname{diag}(k, 1)$ ($k \neq 1$) showing that $M^T Q M = Q$ holds while $M^T M \neq I$ (`\|M\|_2 > 1`). Why does `Dirac::build` conjugate $M$ by $Q^{1/2}$ and $Q^{-1/2}$ inside its power-iteration loop when computing `gain_q`?

