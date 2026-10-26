---
title: "Module 5 — Resonance overlap, cantori and phase-space diffusion"
description: "Chirikov's overlap criterion, the breakup of the golden torus, Aubry–Mather cantori and their turnstiles, the regimes of momentum diffusion — on the canvas's Chirikov operator and Suzu's kicked rotor."
---
*Lecture notes written against `midi-sink` as shipped (libsumi 1.12.0, Voxo 0.26.0, 2026-10-25) and checked claim by claim against the engine before this fold (2026-10-26; libsumi is unchanged since, and Voxo's later versions add the sampler's second generation and touch no synth claim here). Code citations and gate numbers refer to `PROJECT_SPEC.md`, `_work/TO_PROJECT_SPEC.md`, `docs/DECISIONS.md` Part XIII (cited `DECISIONS_13 #n`), `voxo/src/suzu.h`, and `site/src/content/docs/operators/chirikov.mdx`. The lab beside the module is a standalone teaching re-implementation, never the engine; the engine's own lab is [the Suzu lab](../../suzu/lab/).*

---

## Lecture Notes

### 5.1 Poincaré–Birkhoff Chains, Homoclinic Tangles & Chirikov's Resonance Overlap Criterion

In Module 4, we proved two complementary theorems for a perturbed integrable Hamiltonian $H(I, \theta) = H_0(I) + \epsilon H_1(I, \theta)$ (or its stroboscopic twist map, the **Chirikov Standard Map** $p_{n+1} = p_n + K\sin\theta_n$, $\theta_{n+1} = \theta_n + p_{n+1}$):
1. **KAM Theorem:** Strongly irrational (Diophantine) tori $|k \cdot \omega| \ge \gamma \|k\|_1^{-\tau}$ survive for small $\epsilon$ (or $K < K_c(\omega)$), acting in 2D phase space as impenetrable topological barriers.
2. **Poincaré–Birkhoff Theorem:** Every rational torus $\omega = r / s \in \mathbb{Q}$ breaks into an alternating necklace of $s$ stable **elliptic periodic points** (surrounded by librating resonant islands) and $s$ unstable **hyperbolic saddle points**.

Now we ask: **how does chaos actually born at those hyperbolic saddle points spread across phase space as $K$ increases, and what remains of a KAM torus after it breaks?**

#### Splitting of Separatrices and the Homoclinic Tangle (Poincaré, 1890)

In an unperturbed 1-DoF pendulum $H_0(\theta, p) = \frac{1}{2}p^2 + K\cos\theta$, the unstable saddle point at $(\theta, p) = (0, 0)$ (equivalent to $(2\pi, 0)$ on the cylinder) is connected to itself by a smooth figure-eight **separatrix** $p = \pm 2\sqrt{K}\sin(\theta/2)$. Every trajectory leaving $(0, 0)$ in the distant past along the **unstable manifold** $W^u(0, 0)$ arrives smoothly back at $(0, 0)$ in the distant future along the **stable manifold** $W^s(0, 0)$; thus $W^u$ and $W^s$ coincide identically ($W^u = W^s$).

Under a discrete kick step or time-periodic perturbation, $W^u$ and $W^s$ are 1-dimensional invariant curves in 2D phase space that **no longer coincide**. Instead, they split apart (by an angle exponentially small in $1/\sqrt{K}$ for small $K$, governed by the **Melnikov–Lazutkin integral**) and cross each other transversely at a **primary homoclinic point** $z_\star \in W^u \cap W^s$!

Consider the geometric consequences of a single transverse intersection $z_\star \in W^u \cap W^s$ under an area-preserving symplectomorphism $M$ ($\det DM = 1$):
1. **Infinite Cascade of Intersections:** Because $W^u$ and $W^s$ are invariant sets ($M(W^u) = W^u$, $M(W^s) = W^s$), every forward and backward iterate $M^n(z_\star)$ ($n \in \mathbb{Z}$) is *also* a transverse homoclinic intersection point!
2. **Symplectic Lobe Area Conservation Forces Exponential Stretching:** Let $L_0$ be the closed region (**homoclinic lobe**) bounded by the segment of $W^u$ and the segment of $W^s$ between $z_\star$ and $M(z_\star)$. Because $M$ is a symplectomorphism, every iterate $L_n = M^n(L_0)$ must enclose the **exact same symplectic area**:
   $$\operatorname{Area}(L_n) = \iint_{L_n} dq \wedge dp = \iint_{L_0} dq \wedge dp = \Delta A_{\text{lobe}} > 0 \qquad \forall n \in \mathbb{Z}$$
   Yet as $n \to +\infty$, the endpoints $M^n(z_\star)$ and $M^{n+1}(z_\star)$ converge exponentially along $W^s$ toward the saddle $(0, 0)$, so the base width of lobe $L_n$ shrinks as $\lambda_-^n \to 0$! To keep its area $\Delta A_{\text{lobe}}$ constant while its width shrinks to zero (and without ever self-intersecting, since $W^u$ is an embedded manifold), **the lobe $L_n$ must stretch exponentially in length and fold back and forth across $W^s$ in a wild, self-similar oscillation** — Poincaré's **homoclinic tangle**!

> In *Les Méthodes Nouvelles de la Mécanique Céleste* (Vol. 3, 1899), Poincaré wrote: *"One will be struck by the complexity of this figure, which I shall not even attempt to draw."* Today, in Part 1 of our WebGL2 lab, your GPU draws 262,144 particles threading through that exact homoclinic tangle at 60 frames per second!

#### Chirikov's Resonance Overlap Criterion (1959, 1979)

When $K$ is small, the homoclinic tangle around each resonant island chain is confined to an exponentially thin stochastic layer sandwiched between surviving KAM tori. How large must $K$ become before adjacent resonant islands collide and destroy the intervening KAM barriers?

Write the Chirikov Standard Map $p_{n+1} = p_n + K\sin\theta_n$, $\theta_{n+1} = \theta_n + p_{n+1}$ as the exact stroboscopic map of the periodically kicked rotor Hamiltonian:

$$H(\theta, p, t) = \frac{1}{2}p^2 + K\cos\theta \sum_{m=-\infty}^{\infty} \delta(t - m) = \frac{1}{2}p^2 + K \sum_{m=-\infty}^{\infty} \cos(\theta - 2\pi m t)$$

where we used the Poisson summation identity $\sum_{m \in \mathbb{Z}} \delta(t - m) = \sum_{m \in \mathbb{Z}} e^{-2\pi i m t}$. Look at the Fourier harmonics $\cos(\theta - 2\pi m t)$:
- Each integer $m \in \mathbb{Z}$ defines a **primary $1:1$ resonance** where the phase $\psi_m = \theta - 2\pi m t$ is stationary: $\dot{\theta} = p = 2\pi m =: p_m^*$.
- Neglecting all harmonics except $m$ reduces the Hamiltonian near $p \approx 2\pi m$ to the single pendulum $H_m(\psi_m, \delta p) = \frac{1}{2}(\delta p)^2 + K\cos\psi_m$, whose separatrix has **momentum half-width**:
  $$\Delta p_{\text{sep}} = 2\sqrt{K}$$
- The spacing in momentum between two neighbouring integer resonances ($p_0^* = 0$ and $p_1^* = 2\pi$) is $\delta p_{01} = 2\pi$.

Boris Chirikov defined the **Resonance Overlap Parameter** $s_{\text{overlap}}$ as the ratio of the sum of the two unperturbed separatrix half-widths to the distance between the resonance centres:

$$\boxed{\; s_{\text{overlap}} := \frac{\Delta p_{\text{sep}}^{(0)} + \Delta p_{\text{sep}}^{(1)}}{|p_1^* - p_0^*|} = \frac{2\sqrt{K} + 2\sqrt{K}}{2\pi} = \frac{2\sqrt{K}}{\pi} \;}$$

- **Naive First-Order Overlap ($s_{\text{overlap}} = 1$):** Setting $2\sqrt{K}/\pi = 1$ predicts global chaos at $K_1 = (\pi/2)^2 \approx 2.467$.
- **Secondary Resonance Hierarchy ($K_c \approx 0.971635$):** Why does the last golden KAM curve actually break earlier, at **Greene's threshold $K_c \approx 0.971635$** (Greene 1979 — the engine quotes no $K_c$: it caps a canvas throw at $K = 1.25$ a step and the rotor's $K$ at 2.5 on the wheel)? Because at second order in perturbation theory, the interaction between the $m=0$ and $m=1$ harmonics generates a **period-2 ($1:2$) resonance** halfway between them at $p = \pi$ with half-width $\Delta p_{1/2} = K/2$, plus an infinite hierarchy of Fibonacci resonances $F_{k-1}/F_k$ whose separatrices overlap at $K_c \approx 0.9716354$!

---

### 5.2 Aubry–Mather Theory, Cantori & MacKay–Meiss–Percival Turnstiles

What happens to an irrational KAM torus $\mathcal{T}_\omega$ the moment $K$ exceeds its critical breakup threshold $K_c(\omega)$? Does the quasi-periodic orbit with rotation number $\omega$ vanish into thin air?

In 1982–1983, Serge Aubry (studying the Frenkel–Kontorova crystal dislocation model) and John Mather (studying monotone area-preserving twist maps) independently proved one of the deepest theorems in modern Hamiltonian dynamics: **the quasi-periodic motion never disappears; when a KAM curve breaks, it turns into an invariant Cantor set — a Cantorus!**

#### Variational Principle & The Aubry–Mather Theorem

Recall from Module 3 (§3.3) that the Chirikov Standard Map (and any monotone twist map where $\frac{\partial \theta_{n+1}}{\partial p_n} > 0$) is generated by a Type-1 generating function $S(\theta_n, \theta_{n+1})$:

$$S(\theta_n, \theta_{n+1}) = \frac{1}{2}(\theta_{n+1} - \theta_n)^2 + V(\theta_n), \qquad V(\theta) = K\cos\theta$$

so that $p_n = -\frac{\partial S(\theta_n, \theta_{n+1})}{\partial \theta_n}$ and $p_{n+1} = \frac{\partial S(\theta_n, \theta_{n+1})}{\partial \theta_{n+1}}$. Consequently, a bi-infinite sequence of angles $\{\theta_n\}_{n \in \mathbb{Z}}$ is a true dynamical trajectory if and only if every finite segment is a stationary point of the **discrete Lagrangian action**:

$$\mathcal{A}\big[\{\theta_n\}\big] = \sum_{n} S(\theta_n, \theta_{n+1}) \quad \Longleftrightarrow \quad \frac{\partial \mathcal{A}}{\partial \theta_n} = \frac{\partial S(\theta_{n-1}, \theta_n)}{\partial \theta_n} + \frac{\partial S(\theta_n, \theta_{n+1})}{\partial \theta_n} = p_n - (\theta_{n+1} - \theta_n) - V'(\theta_n) = 0$$

Even when $K$ is arbitrarily large so that no smooth KAM curves exist, Aubry and Mather proved that the global minimum of $\mathcal{A}$ over the space of monotone configurations with mean spacing $\lim_{N\to\infty}(\theta_N - \theta_0)/N = 2\pi\omega$ **always exists**:

> **Theorem 5.1 (Aubry–Mather Theorem, 1982):** Let $M$ be an exact area-preserving monotone twist map of the cylinder $\mathbb{T}^1 \times \mathbb{R}$. For **every** rotation number $\omega \in \mathbb{R}$ and **every** perturbation strength $K \ge 0$, there exists a closed, $M$-invariant action-minimizing set $\mathcal{M}_\omega \subset \mathbb{T}^1 \times \mathbb{R}$ projecting injectively onto $\mathbb{T}^1$ (a Lipschitz graph $p = P_\omega(\theta)$ over $\pi_1(\mathcal{M}_\omega) \subset \mathbb{T}^1$) such that every orbit in $\mathcal{M}_\omega$ preserves cyclic angular order and has exact rotation number $\omega$:
> 1. **Subcritical ($K < K_c(\omega)$, $\omega$ irrational):** $\mathcal{M}_\omega$ is a smooth, connected **KAM circle** ($\pi_1(\mathcal{M}_\omega) = \mathbb{T}^1$) with zero gaps.
> 2. **Supercritical ($K > K_c(\omega)$, $\omega$ irrational):** $\mathcal{M}_\omega$ is a nowhere-dense, perfect **Cantor set** ($\pi_1(\mathcal{M}_\omega) \subsetneq \mathbb{T}^1$) — called an **Aubry–Mather Cantorus** (Percival, 1979)!

#### MacKay–Meiss–Percival Turnstiles & Symplectic Lobe Flux $\Delta W$ (1984)

Think about what a **cantorus** $\mathcal{M}_\omega$ looks like in phase space for $K$ slightly above $K_c(\omega_{\text{gold}}) \approx 0.971635$:
- Because $\mathcal{M}_\omega$ is a Cantor set on a curve wrapping around the cylinder, its complement along that curve is a countable union of open **gaps** $\{G_n\}_{n \in \mathbb{Z}}$, where $G_{n+1} = M(G_n)$ is the image of gap $G_n$ under one step of the map!
- Because the total circumference of the cylinder is finite ($2\pi$), the sum of all gap lengths $\sum_{n=-\infty}^\infty |G_n| \le 2\pi$ converges, meaning **the gap widths $|G_n|$ shrink exponentially to zero as $n \to \pm\infty$**!

In 1984, Robert MacKay, James Meiss, and Ian Percival asked: **if we close all the gaps of a cantorus to form a continuous closed loop $\mathcal{C}_\omega$ around the cylinder, how much phase-space area leaks across $\mathcal{C}_\omega$ per iteration?**

Here is their famous **Turnstile Construction**:
1. Let $z_n^-, z_n^+ \in \mathcal{M}_\omega$ be the left and right endpoints of gap $G_n$. As $n \to +\infty$, $|z_n^+ - z_n^-| \to 0$ along the **stable manifold** $W^s$ of the gap; as $n \to -\infty$, $|z_n^+ - z_n^-| \to 0$ along the **unstable manifold** $W^u$ of the gap.
2. Join $z_n^-$ to $z_n^+$ along the unstable manifold segment $W^u_n$ for all $n \le 0$, and along the stable manifold segment $W^s_n$ for all $n > 0$. This forms a continuous closed barrier curve $\mathcal{C}_\omega$ around the cylinder!
3. Under one iteration of the map $M$, every gap segment $W^u_{n-1}$ ($n \le 0$) maps *exactly* onto $W^u_n$, and every segment $W^s_n$ ($n \ge 1$) maps *exactly* onto $W^s_{n+1}$ — **except at the widest gap $G_0$!**
   Inside gap $G_0$, the unstable segment $W^u_0$ and the stable segment $W^s_0$ cross at a homoclinic point (a **minimax orbit** $\{\theta_n^{\text{max}}\}$ sitting in the middle of the gap!), forming a figure-eight **Turnstile** with two lobes:
   - **Upward Lobe $L_{\text{up}}$:** Points that sit *below* $\mathcal{C}_\omega$ at step $n$ and land *above* $\mathcal{C}_\omega$ at step $n+1$.
   - **Downward Lobe $L_{\text{down}}$:** Points that sit *above* $\mathcal{C}_\omega$ at step $n$ and land *below* $\mathcal{C}_\omega$ at step $n+1$.

By Stokes' Theorem and exact symplecticity ($\oint_{\mathcal{C}_\omega} p\,dq = \oint_{M(\mathcal{C}_\omega)} p\,dq$), the upward and downward turnstile lobes have **identical symplectic area**, given by **Mather's $\Delta W$ action difference** between the gap-centre minimax orbit $\{\theta_n^{\text{max}}\}$ and the cantorus endpoint minimizing orbit $\{\theta_n^{\text{min}}\}$:

$$\boxed{\; \Delta W(\omega) := \operatorname{Area}(L_{\text{up}}) = \operatorname{Area}(L_{\text{down}}) = \oint_{\text{turnstile lobe}} p\,dq = \sum_{n=-\infty}^{\infty} \Big( S\big(\theta_n^{\text{max}}, \theta_{n+1}^{\text{max}}\big) - S\big(\theta_n^{\text{min}}, \theta_{n+1}^{\text{min}}\big) \Big) \;}$$

Look at how $\Delta W(\omega_{\text{gold}})$ behaves across Greene's critical threshold $K_c \approx 0.971635$:
- For **$K \le K_c$**, the golden KAM torus has no gaps: $\Delta W = 0$ (flux is strictly zero!).
- For **$K > K_c$**, renormalization group theory at the golden fixed point proves that $\Delta W$ turns on with a universal critical exponent $\eta \approx 3.011722$:
  $$\Delta W(\omega_{\text{gold}}) \approx C\,(K - K_c)^{3.011722}$$
  Because the exponent is $\eta \approx 3$, just above the breakup threshold (say at $K = 1.05$, where $K - K_c \approx 0.078$), the turnstile area $\Delta W \propto (0.078)^3 \approx 4.7 \times 10^{-4}$ is **less than one twentieth of one percent of the phase-space area**!
  Therefore, even though the golden KAM curve is technically broken at $K = 1.05$, trajectories initialized below the golden cantorus are **trapped behind the cantorus for thousands of iterations**, queuing up until they happen to land inside the tiny turnstile lobe $L_{\text{up}}$ and pivot across!

---

### 5.3 Normal vs. Anomalous Action Diffusion ($\langle \Delta I^2(t) \rangle \sim D t^\alpha$)

Suppose we initialize a large ensemble of $M$ particles along a single action line $I(0) = I_0$ (with uniform angles $\theta_m(0) \in [0, 2\pi)$) and iterate the symplectic map for $t = N$ steps. How does the mean-squared action spread

$$\sigma_I^2(t) := \big\langle \big(I(t) - \langle I(t) \rangle\big)^2 \big\rangle \sim D\,t^\alpha$$

evolve over time? Depending on $K$ and the local cantorus/island hierarchy, Hamiltonian phase space exhibits **four distinct transport regimes**:

| Transport Regime | Kick Range $K$ | Scaling Exponent $\alpha$ | Physical & Geometric Mechanism |
| :--- | :--- | :--- | :--- |
| **1. KAM Confinement (Bounded)** | $K < K_c \approx 0.9716$ | $\alpha = 0$ as $t \to \infty$<br>($\sigma_I^2(t) \le \sigma_{\max}^2$) | Unbroken golden-ratio KAM curves ($\Delta W = 0$) partition the cylinder into isolated bands. $\sigma_I^2(t)$ saturates at a finite ceiling! |
| **2. Cantorus Turnstile Trapping (Subdiffusion)** | $K_c < K \lesssim 1.4$ | $0 < \alpha < 1$<br>*(transient / multiscale)* | Trajectories are bottlenecked behind golden and noble cantori with tiny turnstile flux $\Delta W \propto (K - K_c)^{3.01}$, pausing on quasi-stationary plateaus before leaking through. |
| **3. Chaotic Sea (Normal Quasilinear Diffusion)** | $K \gtrsim 1.5$ *(away from accelerator modes)* | $\alpha = 1$<br>($\sigma_I^2(t) = 2 D(K)\,t$) | Phases $\theta_n$ decorrelate rapidly; $\Delta p = \sum_{n=0}^{t-1} K\sin\theta_n$ behaves like a diffusive random walk with $D_{\text{QL}} = K^2 / 4$ (modulated by Bessel correlations $J_2(K)$). |
| **4. Sticky Islands & Accelerator Modes (Superdiffusion)** | Near $K \approx 2\pi m$ *(or island boundary stickiness)* | $1 < \alpha \le 2$<br>*(Lévy flights)* | Orbits sticking to cantorus hierarchies around **accelerator-mode islands** step ballistically by $\Delta p = \pm 2\pi$ per kick, producing heavy-tailed Lévy flights! |

#### Derivation of the Quasilinear Diffusion Coefficient $D_{\text{QL}}$ & Rechester–White Oscillations

After $N$ steps of the Chirikov Standard Map, the total momentum change is $\Delta p_N = p_N - p_0 = K \sum_{n=0}^{N-1} \sin\theta_n$. Squaring and averaging over the uniform initial angle ensemble $\theta_0 \in [0, 2\pi)$ gives the exact Green–Kubo / Taylor diffusivity sum:

$$\big\langle \Delta p_N^2 \big\rangle = K^2 \sum_{n=0}^{N-1} \big\langle \sin^2\theta_n \big\rangle + 2 K^2 \sum_{0 \le n < m \le N-1} \big\langle \sin\theta_n \sin\theta_m \big\rangle$$

1. **Random-Phase (Quasilinear) Approximation ($D_{\text{QL}} = K^2 / 4$):**
   If we neglect multi-step angle correlations ($\langle \sin\theta_n \sin\theta_m \rangle \approx 0$ for $n \neq m$) and use $\langle \sin^2\theta_n \rangle = \frac{1}{2}$, we obtain linear Fickian growth $\langle \Delta p_N^2 \rangle = \frac{1}{2}K^2 N = 2 D_{\text{QL}} N$, where:
   $$D_{\text{QL}}(K) := \frac{\langle \Delta p_N^2 \rangle}{2 N} = \frac{K^2}{4}$$
2. **Rechester–White Fourier-Path / Bessel Correlation Correction (1980):**
   Even at large $K$, $\theta_{n+2} = \theta_n + 2p_{n+1} + K\sin(\theta_n + p_{n+1})$ retains a non-zero 2-step correlation $\langle \sin\theta_n \sin\theta_{n+2} \rangle = -\frac{1}{2}J_2(K)$ (where $J_m(K)$ is the Bessel function of order $m$)! Summing the leading Fourier paths gives the famous **Rechester–White oscillatory diffusion coefficient**:
   $$\boxed{\; D(K) = \frac{K^2}{2}\left[ \frac{1}{2} - J_2(K) - J_1^2(K) + J_2^2(K) + J_3^2(K) + \cdots \right] \;}$$
   Notice that near the peaks of $-J_2(K)$ (and at $K \approx 2\pi m \approx 6.283$ where accelerator modes appear), $D(K)$ spikes far above the quasilinear parabola $K^2/4$!

#### How `libsumi` & `Suzu` Gate Chaotic Diffusion (`chirikov.mdx`, `suzu::Rotor`)

Look at [the Chirikov page](../../operators/chirikov/) and `suzu::Rotor` (`voxo/src/suzu.h`):
- On the `libsumi` canvas, `chirikov_fs` applies the standard map driven by **mod-wheel deltas** $\delta$ (so a slow wheel sweep applies many tiny steps with $\delta^2 K_{\max} \ll K_c$, sliding ink smoothly along integrable sheets, while a fast one-frame throw jumps across $K_c \approx 0.9716$ into cantorus turnstiles and homoclinic filaments!). Because chaotic stretching above $K_c$ narrows filaments down to sub-texel width, `libsumi` caps per-step throws in the core at **`K = 1.25`** — right inside the cantorus turnstile regime!
- In `Suzu`, `suzu::Rotor` (`voxo/src/suzu.h`) runs a single trajectory $(\theta_n, p_n)$ at one kick per fundamental period $T_0 = 1/f_0$ and re-bases its magic-circle cell onto $f_0(1 + p_n / 2\pi)$. In Part 2 of this module's lab, we generalize `suzu::Rotor` to a **512-Trajectory Granular Action-Diffusion Synthesizer**: each particle $m \in \{0, \dots, 511\}$ carries its own action $I_m(t)$ and angle $\theta_m(t)$, mapping $I_m(t)$ to oscillator frequency $f_m(t) = f_0 \exp(\gamma I_m(t))$ and $\theta_m(t)$ to stereo pan $\cos\theta_m(t)$!

---

## Lab Instructions (`module_05_lab.html`)

Open [the module's lab](../module_05_lab.html) (a standalone page — a teaching re-implementation, never the engine) in a WebGL2 + WebAudio browser. Both workspaces run live ensemble simulations of the Chirikov Standard Map across the KAM / Cantorus / Diffusion transition.

### Part 1: Shader Lab — 262,144-Particle GPU Turnstile Flux & Density Diffusion

The top workspace uses **two alternating $512 \times 512$ `RG32F` WebGL2 floating-point textures** (`512 × 512 = 262,144` particles!) to integrate a quarter-million trajectories in parallel on the GPU:
- Each texel $(u, v)$ stores a particle's exact floating-point phase-space coordinates $(q_m, p_m)$ (`RG32F`).
- At $t = 0$, all 262,144 particles are initialized along a single horizontal action line $p = I_0$ (default $I_0 = \pi$, right between the two Golden-Ratio KAM / Cantorus barriers at $p_{\text{gold}^-} = 2\pi(1 - \varphi^{-1}) \approx 2.400$ and $p_{\text{gold}^+} = 2\pi\varphi^{-1} \approx 3.883$!).
- A GPU **Point-Splatting Additive Histogram Shader** (`gl.POINTS` with additive blending `gl.ONE, gl.ONE`) renders all 262,144 particles onto the phase portrait alongside the golden cantorus barriers and turnstile lobe markers!

1. **Watch Cantorus Trapping & Turnstile Leakage ($K = 1.05$ vs. $K = 0.85$ vs. $K = 1.60$):**
   - Click **$K = 0.85$ (KAM Trapped)**: All 262,144 particles spread inside the central stochastic zone but **hit a brick wall** at the two gold horizontal curves ($p \approx 2.40$ and $p \approx 3.88$). Notice in the HUD that **Cantorus Leakage %** stays at **`0.00%`** forever, and $\langle \Delta I^2(t) \rangle$ saturates flat!
   - Now click **$K = 1.06$ (Cantorus Turnstile Leak)** and click **Reset 262,144 Line**: Here $K > K_c \approx 0.9716$, so the golden KAM curves have broken into **Aubry–Mather Cantori** with tiny MacKay–Meiss–Percival turnstile lobes of area $\Delta W \propto (K - K_c)^{3.01} \approx 5 \times 10^{-4}$! Watch the density histogram: **the 262,144 particles pile up behind the golden cantorus barrier for hundreds of iterations**, and then thin jets of particles begin **leaking through the turnstile gap** (marked by the glowing turnstile portals) into the upper and lower cylinders!
   - Click **$K = 1.65$ (Normal Diffusion)** or **$K = 6.35$ (Accelerator Superdiffusion)**: Watch the cantorus dam wash away as the cloud diffuses across the cylinder with $\langle \Delta I^2(t) \rangle \sim D t^\alpha$, and read the live power-law exponent $\alpha$ and empirical diffusivity $D_{\text{emp}}$ vs. $D_{\text{QL}} = K^2/4$ on the **Log–Log Variance $\langle \Delta I^2(t) \rangle$ Scope**!

### Part 2: Sound Lab — 512-Voice Granular Action-Diffusion Synthesizer at 48 kHz

The bottom workspace evolves an ensemble of **512 diffusing trajectories** $\{(I_m(t), \theta_m(t))\}_{m=0}^{511}$ all initialized along $I_m(0) = 0$ (with tiny thermal spread $10^{-4}$ and uniform angles $\theta_m(0) = 2\pi m / 512$) and synthesizes all **512 sinusoidal oscillators in real time at 48 kHz**:

$$f_m(t) = f_0 \exp\!\big(\gamma\,I_m(t)\big), \qquad \operatorname{pan}_m(t) = \cos\!\big(\theta_m(t)\big) \in [-1, +1]$$

1. **Hear the Unison Tone Fracture Across $K_c \approx 0.9716$:**
   - Click **Start 512-Voice Granular Synth** and click **Reset 512 Unison ($I_m = 0$)** at **$K = 0.00$**. All 512 voices sit on the exact same action line $I_m = 0$, playing a **pure, laser-focused unison sine tone** at $f_0 = 220\text{ Hz}$ while rotating smoothly around the stereo field!
   - Raise $K$ to **`0.75` (Subcritical KAM)**: The unison fractures into a **bounded, shimmering chorus** trapped strictly between the inner KAM barriers — the pitch spread never widens past the KAM dam!
   - Raise $K$ to **`1.05` (Cantorus Turnstile Pausing)** and click **Reset 512 Unison**: Listen closely! First, the 512 voices spread into a tight cluster trapped behind the first cantorus barrier — holding a **stationary microtonal cluster chord** for several seconds — and then, as trajectories squeeze one by one through the MacKay–Meiss–Percival turnstile lobes, you hear discrete **microtonal tendril voices** peel away upward and downward into the next cantorus chamber!
   - Raise $K$ to **`1.55` (Global Chaotic Cloud)**: The cantori dissolve and the 512 voices diffuse outward as $\exp(\gamma I_m(t))$ into a vast, stereo-panned **stochastic Ligeti-style cloud**!

---

## Problem Set

### Problem 5.1: Chirikov's Resonance Overlap Criterion & Secondary $1:2$ Islands

Consider the continuously driven rotor Hamiltonian $H(\theta, p, t) = \frac{1}{2}p^2 + K\cos\theta \sum_{m=-\infty}^\infty \cos(2\pi m t)$.

1. **Primary Integer Resonances & Overlap Threshold:**
   Near the $m$-th primary resonance $p \approx 2\pi m$, define the resonant phase $\psi_m = \theta - 2\pi m t$ and momentum deviation $\delta p = p - 2\pi m$.
   - Average $H$ over the fast non-resonant terms $m' \neq m$ to obtain the pendulum Hamiltonian $H_m(\psi_m, \delta p) = \frac{1}{2}(\delta p)^2 + K\cos\psi_m$.
   - Show that the maximum momentum excursion on the separatrix $H_m = +K$ is $\Delta p_{\text{half}} = 2\sqrt{K}$, and verify that two adjacent integer resonances ($m$ and $m+1$, separated by $\delta p = 2\pi$) touch when:
     $$s_{\text{overlap}} = \frac{2\Delta p_{\text{half}}}{2\pi} = \frac{2\sqrt{K}}{\pi} = 1 \quad \Longleftrightarrow \quad K_{\text{overlap}}^{(1)} = \frac{\pi^2}{4} \approx 2.4674$$

2. **Secondary $1:2$ Resonance at $p = \pi$:**
   Using canonical perturbation theory to second order in $K$ midway between $m = 0$ and $m = 1$ (at $p = \pi$), the interaction of $\cos\theta$ and $\cos(\theta - 2\pi t)$ produces an effective half-integer resonance in $2\theta - 2\pi t$ whose separatrix half-width is $\Delta p_{1/2} = K/2$ (§5.1; derive the resonant potential's amplitude on the way). Show that requiring the primary separatrices ($\Delta p_0 = 2\sqrt{K}$) and the secondary $1:2$ separatrix ($\Delta p_{1/2} = K/2$) to fill the half-spacing $\pi$ gives the improved overlap equation:
   $$2\sqrt{K} + \frac{K}{2} = \pi \quad \Longrightarrow \quad K_{\text{overlap}}^{(2)} \approx 1.46$$
   Explain how including the infinite Fibonacci chain of higher-order resonances $F_{k-1}/F_k$ around $\omega_{\text{gold}} = \varphi^{-1}$ brings the overlap threshold down to Greene's exact $K_c \approx 0.971635$.

---

### Problem 5.2: Discrete Lagrangian Action, Cantori & Turnstile Flux $\Delta W$

The Chirikov Standard Map $p_{n+1} = p_n + K\sin\theta_n$, $\theta_{n+1} = \theta_n + p_{n+1}$ is generated by the Type-1 generating function:

$$S(\theta_n, \theta_{n+1}) = \frac{1}{2}(\theta_{n+1} - \theta_n)^2 + K\cos\theta_n$$

1. **Period-1 & Period-2 Action-Minimizing vs. Minimax Orbits:**
   For rotation number $\omega = 0/1$ ($\theta_{n+1} = \theta_n$), the 1-step action is $W(\theta_0) = S(\theta_0, \theta_0) = K\cos\theta_0$.
   - Find the action-minimizing fixed point $\theta_0^{\text{min}}$ and the minimax (saddle/maximum) fixed point $\theta_0^{\text{max}}$, and compute the linearized Jacobian trace $\operatorname{tr}(DM)$ at each to verify that the action minimum at $\theta_0^{\text{min}} = \pi$ is hyperbolic ($\operatorname{tr} DM = 2 + K > 2$ for $K > 0$), while $\theta_0^{\text{max}} = 0$ is elliptic ($\operatorname{tr} DM = 2 - K \in (-2, 2)$ for $0 < K < 4$).
   - Explain why Poincaré–Birkhoff island chains always pair an action-minimizing hyperbolic saddle with an action-minimax elliptic centre!

2. **Exact Turnstile Lobe Area from Generating Functions (MacKay–Meiss–Percival Theorem):**
   Suppose $z^{\text{min}}_n = (\theta_n^{\text{min}}, p_n^{\text{min}})$ and $z^{\text{max}}_n = (\theta_n^{\text{max}}, p_n^{\text{max}})$ are two homoclinic orbits (such as the gap endpoints and gap-centre minimax point of a cantorus) that converge to the same asymptotic limits as $n \to \pm\infty$, connected by an unstable curve $\gamma_u$ at step 0 and a stable curve $\gamma_s$ at step 0 that bound a turnstile lobe $L$. Using $p_n\,d\theta_n - p_{n+1}\,d\theta_{n+1} = -dS(\theta_n, \theta_{n+1})$ and telescoping the integral from $n = -M$ to $n = +M$, prove that the symplectic area of the turnstile lobe is given by Mather's $\Delta W$ sum:
   $$\operatorname{Area}(L) = \oint_{\partial L} p\,d\theta = \sum_{n=-\infty}^{\infty} \Big( S\big(\theta_n^{\text{max}}, \theta_{n+1}^{\text{max}}\big) - S\big(\theta_n^{\text{min}}, \theta_{n+1}^{\text{min}}\big) \Big)$$

---

### Problem 5.3: Quasilinear Diffusion, Bessel Correlations & Accelerator Modes

1. **Two-Step Angle Correlation & The $-J_2(K)$ Correction (Rechester & White, 1980):**
   Starting from a uniform distribution of $(\theta_0, p_0) \in [0, 2\pi)^2$, write $\theta_1 = \theta_0 + p_0 + K\sin\theta_0$ and $\theta_2 = \theta_1 + p_0 + K\sin\theta_0 + K\sin\theta_1$.
   - Using the Jacobi–Anger Bessel identity $e^{i z \sin\phi} = \sum_{m=-\infty}^\infty J_m(z)\,e^{i m \phi}$, integrate over $(\theta_0, p_0) \in [0, 2\pi)^2$ to prove that:
     $$\big\langle \sin\theta_0\,\sin\theta_1 \big\rangle = 0, \qquad \big\langle \sin\theta_0\,\sin\theta_2 \big\rangle = -\frac{1}{2}J_2(K)$$
   - Substitute this into the Green–Kubo diffusivity formula $D(K) = \frac{K^2}{2}\Big(\frac{1}{2} + \sum_{m=1}^\infty \langle \sin\theta_0 \sin\theta_m \rangle\Big)$ to obtain the leading oscillatory diffusion law:
     $$D(K) \approx \frac{K^2}{4}\big(1 - 2 J_2(K)\big)$$

2. **Ballistic Accelerator Modes & Superdiffusion ($\alpha = 2$):**
   Look for a period-1 **accelerator mode** of the Chirikov Standard Map where the angle returns to the same value $\theta_{n+1} = \theta_n = \theta_* \pmod{2\pi}$ while the momentum increases by $+2\pi m$ every single step ($p_{n+1} = p_n + 2\pi m$, $m \in \mathbb{Z} \setminus \{0\}$).
   - Show that such a fixed point exists at $p_* = 0 \pmod{2\pi}$ and $\sin\theta_* = \frac{2\pi m}{K}$ whenever $K \ge 2\pi |m|$.
   - Compute the Jacobian trace $\operatorname{tr}(DM) = 2 + K\cos\theta_*$ at this accelerator point and prove that it is **linearly stable (elliptic, $|\operatorname{tr} DM| < 2$)** inside the window:
     $$2\pi |m| < K < \sqrt{(2\pi m)^2 + 16}$$
   - Explain why trajectories initialized inside (or sticking to the cantori around) this stable island have $\Delta p_N \propto 2\pi m N$, driving anomalous superdiffusion $\langle \Delta p_N^2 \rangle \sim N^\alpha$ with $\alpha \to 2$.

