# Module 6: High-Dimensional Dynamics & Arnold Diffusion (Graduate Capstone)

*Target codebase: `midi-sink` as shipped (`libsumi 1.12.0, Voxo 0.26.0`, 2026-10-25). All code citations and gate numbers refer to `PROJECT_SPEC.md`, `_work/TO_PROJECT_SPEC.md`, `_work/DECISIONS_13.md`, `voxo/src/suzu.h`, and `site/src/content/docs/operators/`.*

---

## Lecture Notes

### 6.1 Topological Obstruction ($n = 2$) vs. Escape ($n \ge 3$): Why KAM Tori Cannot Trap Orbits in High Dimensions

In Modules 4 and 5, we studied the **Kolmogorov–Arnold–Moser (KAM) Theorem** and saw that for an autonomous 2-DoF Hamiltonian system (or a 1-DoF time-periodic twist map like the Chirikov Standard Map), surviving Diophantine KAM tori act as **impenetrable topological dams** for $K < K_c \approx 0.971635$. Even though resonant islands and chaotic homoclinic tangles form a dense web between the KAM curves, no chaotic trajectory can ever cross a surviving KAM curve, so the action variables remain permanently trapped near their initial values: $\|I(t) - I(0)\| = \mathcal{O}(\sqrt{\epsilon})$ for all $t \in (-\infty, +\infty)$.

In 1964, Vladimir Arnold discovered a startling dimensional divide: **this topological trapping works ONLY for $n \le 2$ degrees of freedom! For $n \ge 3$ degrees of freedom, KAM tori no longer divide the energy surface, and chaotic trajectories can wander across the entire phase space for arbitrarily small perturbations $\epsilon > 0$.**

#### The Codimension Formula on the Energy Hypersurface

Let $(M^{2n}, \omega, H)$ be an autonomous Hamiltonian system with $n$ degrees of freedom:
- Conservation of total energy $H(q, p) = E$ restricts any trajectory to a regular **isoenergetic hypersurface** $\Sigma_E := H^{-1}(E) \subset M^{2n}$ of dimension:
  $$\dim \Sigma_E = 2n - 1$$
- By the Liouville–Arnold and KAM theorems (Module 4, §4.1–§4.2), every surviving invariant KAM torus $\mathcal{T}_\omega \subset \Sigma_E$ is a **Lagrangian submanifold** of dimension:
  $$\dim \mathcal{T}_\omega = n$$
- Therefore, the **codimension** of a KAM torus $\mathcal{T}_\omega$ inside the energy hypersurface $\Sigma_E$ is:
  $$\boxed{\; \operatorname{codim}_{\Sigma_E}(\mathcal{T}_\omega) = \dim \Sigma_E - \dim \mathcal{T}_\omega = (2n - 1) - n = n - 1 \;}$$

By the **Jordan–Brouwer Separation Theorem** (and Alexander Duality in algebraic topology), a closed submanifold separates an ambient manifold into disconnected "inside" and "outside" regions **if and only if its codimension is equal to $1$**! Let us compare the dimensions side by side:

| Degrees of Freedom $n$ | Phase Space $\dim M = 2n$ | Energy Surface $\dim \Sigma_E = 2n - 1$ | KAM Torus $\dim \mathcal{T}_\omega = n$ | Codimension in $\Sigma_E$: $n - 1$ | Topological Consequence & Stability |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **$n = 1$** *(Autonomous Pendulum / Oscillator)* | $2$ | $1$ | $1$ | **$0$** | $\mathcal{T}_\omega = \Sigma_E$: the energy curve *is* the invariant 1-torus. Completely integrable. |
| **$n = 1\tfrac{1}{2}$** *(Periodically Kicked Rotor / Standard Map)* | $2$ *(per strobe)* | $2$ *(cylinder $\mathbb{T}^1 \times \mathbb{R}$)* | $1$ *(circle $\mathbb{T}^1$)* | **$1$** | 1D circles separate the 2D cylinder. **Topological trapping** for $K < K_c$. |
| **$n = 2$** *(Hénon–Heiles, Stadium Billiard, Double Pendulum)* | $4$ | $3$ | $2$ *(2-torus $\mathbb{T}^2$)* | **$1$** | 2D tori $\mathbb{T}^2$ are closed surfaces inside $\Sigma_E^3$ (like nested doughnuts in $\mathbb{R}^3$). **Topological trapping:** $\|I(t) - I(0)\| = \mathcal{O}(\sqrt{\epsilon})$ forever! |
| **$n = 3$** *(3-Torus Modal Lattice, 5-Vortex Braid, Asteroid Belt)* | $6$ | $5$ | $3$ *(3-torus $\mathbb{T}^3$)* | **$2$** | Codimension $5 - 3 = 2$ (like 1D curves/circles in $\mathbb{R}^3$!). **No topological trapping:** $\Sigma_E^5 \setminus \bigcup_\omega \mathcal{T}_\omega$ is **path-connected**! |
| **General $n \ge 3$** *(Suzu 16-Mode Lattice, Toda Perturbations)* | $2n$ | $2n - 1$ | $n$ | **$n - 1 \ge 2$** | Complement of all KAM tori is a single connected network (**Arnold Web**). Global **Arnold Diffusion** occurs for any $\epsilon > 0$! |

Think of the analogy in ordinary 3D space $\mathbb{R}^3$:
- A **2D spherical shell** (codimension $3 - 2 = 1$) completely seals off its interior from its exterior; a point inside can never escape without piercing the shell. That is **$n = 2$** ($\mathbb{T}^2 \subset \Sigma_E^3$).
- A **1D closed loop of wire** (codimension $3 - 1 = 2$) cannot trap a point in $\mathbb{R}^3$! Even if you fill a room with a dense forest of 1D wires of positive total measure, a trajectory can always thread its way *around* the wires without touching a single one. That is **$n = 3$** ($\mathbb{T}^3 \subset \Sigma_E^5$)!

#### How High-Dimensional Phase Spaces Appear in `midi-sink`

1. **$N$-Vortex Kirchhoff Chord Braiding (`SUMI_DEFORM_BRAID`, `_work/TO_PROJECT_SPEC.md §10.8.2`):**
   Recall from Module 2 (§2.1) that in `libsumi`'s *free* chord-braiding mode (`braid_free = 1`), $N$ held MIDI notes spawn $N$ interacting point vortices governed by Kirchhoff's Hamiltonian $H_{\text{Kirchhoff}}(x_1, \dots, x_N)$ on a $2N$-dimensional phase space ($n = N$ degrees of freedom). Because $H_{\text{Kirchhoff}}$ is translation- and rotation-invariant in the plane, Noether's theorem provides 3 independent integrals ($H$, angular impulse $M = \sum_k \Gamma_k \|x_k\|^2$, and two commuting combinations of linear impulse $P_x, P_y$), reducing the system by 2 degrees of freedom to $n_{\text{eff}} = N - 2$:
   - **1, 2, or 3 Held Notes ($N \le 3 \implies n_{\text{eff}} \le 1$):** Complete Liouville integrability! Every 3-vortex triad moves on invariant tori and never exhibits chaos.
   - **4 Held Notes ($N = 4 \implies n_{\text{eff}} = 2$):** Chaotic layers appear, but surviving 2-tori have codimension $1$ inside the reduced 3D energy-impulse manifold, trapping vortex excursions.
   - **5 to 15 Held Notes ($N \ge 5 \implies n_{\text{eff}} = N - 2 \ge 3$):** Invariant tori have codimension $n_{\text{eff}} - 1 \ge 2$, opening the **Arnold Web** across vortex configuration space!
2. **Coupled 16-Mode Resonator Lattice (`lattice_kick` & `lattice_rotate_simd`, `voxo/src/suzu.h` lines 319–390):**
   In `Suzu`, each voice's modal resonator bank couples up to $n = 16$ second-order cells ($\dim M = 32$) via the shared-potential Laplacian kick $y_k \mathrel{+}= (\kappa \varepsilon_0^2 / \varepsilon_k)(L x)_k$ combined with the nonlinear restoring shear $s(y_k / A_k)$. For $n \ge 3$ active modes and $\kappa > 0$, invariant tori have codimension $n - 1 \ge 2$, allowing slow resonant action transfer across the partial bank while the joint symplectic integrator preserves total energy to machine precision.

---

### 6.2 The Arnold Web ($\sum_{i=1}^n k_i \omega_i(I) = 0$), Whiskered Tori & Transition Chains

If trajectories for $n \ge 3$ can bypass the surviving KAM tori, *where* in action space $(I_1, \dots, I_n)$ do they travel? They travel along the resonant channels of the **Arnold Web**!

#### Geometry of the Arnold Web in Action Space $(I_1, I_2, I_3)$

Consider a 3-DoF near-integrable Hamiltonian $H(I, \theta) = H_0(I_1, I_2, I_3) + \epsilon H_1(I, \theta)$ with unperturbed frequency map:

$$\omega(I) = \nabla H_0(I) = \big(\omega_1(I),\, \omega_2(I),\, \omega_3(I)\big)$$

For every non-zero integer vector $k = (k_1, k_2, k_3) \in \mathbb{Z}^3 \setminus \{0\}$ (with $\gcd(|k_1|, |k_2|, |k_3|) = 1$), the condition of rational commensurability defines a 2-dimensional **resonance surface** $\mathcal{R}_k \subset \mathbb{R}^3$:

$$\mathcal{R}_k := \Big\{ I = (I_1, I_2, I_3) \in \mathbb{R}^3 : k \cdot \omega(I) = k_1 \omega_1(I) + k_2 \omega_2(I) + k_3 \omega_3(I) = 0 \Big\}$$

To make this concrete, take the canonical strictly convex unperturbed Hamiltonian:

$$H_0(I_1, I_2, I_3) = \frac{1}{2}\big(I_1^2 + I_2^2 + I_3^2\big) \quad \Longrightarrow \quad \omega(I) = (I_1, I_2, I_3)$$

Here:
- The unperturbed isoenergetic surface $H_0(I) = E$ in 3D action space is the **sphere** $I_1^2 + I_2^2 + I_3^2 = 2E$ of radius $\sqrt{2E}$.
- Each resonance surface $\mathcal{R}_k$ is the plane $k_1 I_1 + k_2 I_2 + k_3 I_3 = 0$ normal to $k$, which slices the energy sphere $H_0(I) = E$ along a **great circle** $\mathcal{C}_k = \mathcal{R}_k \cap S^2_{\sqrt{2E}}$!
- Under perturbation $\epsilon H_1(I, \theta) = \epsilon \sum_{k} V_k \cos(k \cdot \theta)$, each great circle $\mathcal{C}_k$ thickens into a **resonant strip (channel)** of half-width:
  $$\Delta I_k = 2\sqrt{\frac{\epsilon |V_k|}{\|k\|_2}} \sim \sqrt{\epsilon}\,e^{-\rho \|k\|_1 / 2}$$
- Because all great circles on a sphere intersect one another, the union of all resonant strips $\mathcal{W}_\epsilon := \bigcup_{k \neq 0} \mathcal{C}_k$ forms a **single connected, dense spiderweb covering the energy sphere** — the **Arnold Web**!

Look closely at the **two distinct directions of motion** inside a resonance channel $\mathcal{R}_k$:
1. **Fast Transverse Libration ($\parallel k$, across the channel width $\mathcal{O}(\sqrt{\epsilon})$):**
   If only a single resonant harmonic $\cos(k \cdot \theta)$ were present, then $\dot{I} = \epsilon k V_k \sin(k \cdot \theta)$ would point purely along the integer vector $k$! On the energy sphere $H_0(I) = E$, the vector $k$ is **perpendicular** to the resonance plane $\mathcal{R}_k$ ($\nabla(k \cdot I) = k \perp \mathcal{R}_k$) and tangent to $H_0(I) = E$ precisely on $\mathcal{R}_k$ (since $k \cdot \nabla H_0(I) = k \cdot \omega(I) = 0$ on $\mathcal{R}_k$!). Thus, the single-harmonic pendulum motion oscillates back and forth *across* the narrow width $\mathcal{O}(\sqrt{\epsilon})$ of the channel and can never move *along* the channel!
2. **Slow Longitudinal Arnold Drift ($\perp k$ and $\perp \omega(I)$, along the channel length $\mathcal{O}(1)$):**
   Because $H_0(I)$ is strictly convex ($k^T D^2 H_0(I) k = \|k\|^2 > 0$), moving in the direction of $k$ immediately pushes $k \cdot \omega(I)$ away from zero and detunes the resonance. How, then, does a trajectory travel $\mathcal{O}(1)$ distances along the web? It drifts along the **tangent vector of the resonance curve** $\mathcal{C}_k = \mathcal{R}_k \cap \Sigma_E$:
   $$\boxed{\; \mathbf{t}_k(I) := k \times \omega(I) \;}$$
   Notice that $\mathbf{t}_k(I) = k \times \omega(I)$ is simultaneously orthogonal to $\omega(I) = \nabla H_0(I)$ (so it stays on the constant-energy sphere $H_0(I) = E$!) and orthogonal to $k$ (so it stays locked on the resonance plane $k_1 \omega_1 + k_2 \omega_2 + k_3 \omega_3 = 0$!) until it reaches a **resonance junction (hub)** $\mathcal{R}_{k^{(1)}} \cap \mathcal{R}_{k^{(2)}}$, where $\omega(I)$ is parallel to $k^{(1)} \times k^{(2)}$ and the trajectory can switch onto a new resonance line!

#### Arnold's 1964 Theorem: Whiskered Tori & Transition Chains

How does the slow longitudinal drift along $\mathcal{C}_k$ actually happen mechanically? Through **whiskered tori** and **Arnold transition chains**!

Consider Arnold's celebrated 1964 model Hamiltonian (written in autonomous 3-DoF form with $\dot{\theta}_3 = 1$):

$$H(I_1, I_2, I_3, \theta_1, \theta_2, \theta_3) = \frac{1}{2}I_1^2 + \frac{1}{2}I_2^2 + I_3 + \epsilon\big(\cos\theta_1 - 1\big)\Big(1 + \mu\big(\sin\theta_2 + \cos\theta_3\big)\Big)$$

Let us trace Arnold's mechanism in three steps:
1. **Unsplit Whiskered Tori ($\epsilon > 0, \mu = 0$):**
   When $\mu = 0$, the system decouples into a pendulum $H_{\text{pend}}(I_1, \theta_1) = \frac{1}{2}I_1^2 + \epsilon(\cos\theta_1 - 1)$ along the $(1, 0, 0)$ resonance $\mathcal{R}_{(1,0,0)}$ ($I_1 = 0$) and a 2-torus rotor in $(\theta_2, \theta_3)$ where $I_2 = \omega_2$ is strictly constant!
   For every fixed value of the longitudinal action $I_2 = c$, the hyperbolic saddle point $(I_1 = 0, \theta_1 = 0)$ of the pendulum sweeps out an unstable invariant 2-torus $\mathcal{T}_c = \{I_1 = 0, \theta_1 = 0, I_2 = c, (\theta_2, \theta_3) \in \mathbb{T}^2\}$.
   Because $\mathcal{T}_c$ is hyperbolic in the transverse $(I_1, \theta_1)$ plane and neutral along $\mathbb{T}^2$, it possesses a 3-dimensional **stable manifold** $W^s(\mathcal{T}_c)$ (trajectories approaching $\mathcal{T}_c$ as $t \to +\infty$) and a 3-dimensional **unstable manifold** $W^u(\mathcal{T}_c)$ (trajectories leaving $\mathcal{T}_c$ as $t \to -\infty$). Arnold named $\mathcal{T}_c$ a **whiskered torus**, and $W^s(\mathcal{T}_c), W^u(\mathcal{T}_c)$ its **incoming and outgoing whiskers**.
   When $\mu = 0$, $\dot{I}_2 = 0$ everywhere, so the outgoing whisker $W^u(\mathcal{T}_c)$ coincides identically with the incoming whisker $W^s(\mathcal{T}_c)$ of the *same* torus $c$, and no drift in $I_2$ can occur.
2. **Whisker Splitting & Heteroclinic Transition Chains ($0 < \mu \ll \epsilon \ll 1$):**
   When $\mu > 0$, the coupling term $\epsilon\mu(\cos\theta_1 - 1)(\sin\theta_2 + \cos\theta_3)$ leaves every whiskered torus $\mathcal{T}_c$ completely untouched (because $\cos\theta_1 - 1$ and its gradient vanish to second order at $\theta_1 = 0$!).
   However, along the whiskers $W^u(\mathcal{T}_c)$ and $W^s(\mathcal{T}_{c'})$ away from $\theta_1 = 0$, $\cos\theta_1 - 1 \neq 0$, so the whiskers **split apart**! Using the multi-dimensional **Melnikov potential**, Arnold proved that the outgoing whisker $W^u(\mathcal{T}_{c_m})$ of torus $c_m$ intersects transversely the incoming whisker $W^s(\mathcal{T}_{c_{m+1}})$ of a neighbouring torus $c_{m+1}$ whenever $|c_{m+1} - c_m|$ is smaller than the Melnikov splitting amplitude:
   $$W^u(\mathcal{T}_{c_m}) \pitchfork W^s(\mathcal{T}_{c_{m+1}}), \qquad |c_{m+1} - c_m| \le \Delta I_{\text{split}} \sim \mu\,\exp\!\left(-\frac{\pi\,\omega_2}{2\sqrt{\epsilon}}\right)$$
3. **Shadowing the Chain ($\mathcal{O}(1)$ Drift in $I_2$):**
   A sequence of whiskered tori $\mathcal{T}_{c_0}, \mathcal{T}_{c_1}, \dots, \mathcal{T}_{c_M}$ connected by transverse heteroclinic whisker intersections $W^u(\mathcal{T}_{c_m}) \pitchfork W^s(\mathcal{T}_{c_{m+1}})$ is called an **Arnold Transition Chain**. By Smale's **$\lambda$-Lemma (Inclination Lemma)**, the unstable manifold $W^u(\mathcal{T}_{c_0})$ accumulates on $W^u(\mathcal{T}_{c_1})$, which accumulates on $W^u(\mathcal{T}_{c_2})$, and so on all the way to $\mathcal{T}_{c_M}$! Thus there exists an open neighborhood of initial conditions near $\mathcal{T}_{c_0}$ that shadows the entire transition chain, drifting from $I_2(0) \approx A$ to $I_2(T) \approx B$ ($|B - A| = \mathcal{O}(1)$) for **arbitrarily small** $0 < \mu \ll \epsilon \ll 1$!

---

### 6.3 Nekhoroshev's Exponential Stability Theorem (1977) & Fast Lyapunov Indicators (FLI)

Look at the Melnikov whisker-splitting formula above: the step size $\Delta I_{\text{split}}$ between consecutive tori in Arnold's transition chain contains the factor $\exp(-\pi\omega_2 / 2\sqrt{\epsilon})$, which is **exponentially small in $1/\sqrt{\epsilon}$**! Does this mean *all* Arnold diffusion in analytic quasi-convex systems is exponentially slow?

In 1977, Arnold's student **Nikolai Nekhoroshev** proved that the answer is **yes**: while KAM theory gives perpetual stability ($\forall t \in \mathbb{R}$) for a Cantor set of initial conditions (Diophantine tori), **Nekhoroshev's Theorem** gives **exponentially long stability ($|t| \le T_0 \exp((\epsilon_0/\epsilon)^a)$) for ALL initial conditions in phase space!**

> **Theorem 6.1 (Nekhoroshev's Exponential Stability Theorem, 1977; Lochak–Pöschel 1992–1993):** Let $H(I, \theta) = H_0(I) + \epsilon H_1(I, \theta)$ be real-analytic on $D \times \mathbb{T}^n$, and suppose $H_0(I)$ is **steep** (in particular, **quasi-convex**: on every isoenergetic tangent plane $v \cdot \nabla H_0(I) = 0$, the Hessian quadratic form $v^T D^2 H_0(I) v \ge m \|v\|^2 > 0$ is positive-definite). Then there exist positive constants $\epsilon_0, I_0, T_0 > 0$ such that for all $0 < \epsilon < \epsilon_0$ and **every** initial condition $(I(0), \theta(0)) \in D \times \mathbb{T}^n$:
> $$\boxed{\; \|I(t) - I(0)\| \le I_0\,\epsilon^b \qquad \text{for all } |t| \le T_{\text{Nekh}}(\epsilon) := T_0\,\exp\!\left( \left(\frac{\epsilon_0}{\epsilon}\right)^a \right) \;}$$
> with optimal stability exponents (Lochak 1992, Pöschel 1993):
> $$a = \frac{1}{2n}, \qquad b = \frac{1}{2n}$$

#### Why Nekhoroshev Stability Requires Both Analyticity and Convexity (Geometry of the Proof)

Why does Nekhoroshev's bound hold even right inside the resonant channels $\mathcal{R}_k$ of the Arnold web?
1. **Analytic Ultraviolet Cutoff & Exponential Remainder:**
   Because $H_1(I, \theta) = \sum_{k \in \mathbb{Z}^n} \hat{H}_{1,k}(I) e^{i k \cdot \theta}$ is real-analytic on a strip of width $\rho$, its Fourier harmonics decay exponentially: $|\hat{H}_{1,k}| \le C e^{-\rho \|k\|_1}$. If we truncate the Fourier series at an ultraviolet cutoff $\|k\|_1 \le K_{\text{UV}} \sim \epsilon^{-a}$ ($a = 1/2n$), the truncated high-frequency tail is already **exponentially small**: $\mathcal{O}\big(\exp(-(\epsilon_0/\epsilon)^a)\big)$.
2. **Covering Action Space by Resonant Blocks (Geography of Resonances):**
   For the finitely many low-order harmonics $\|k\|_1 \le K_{\text{UV}}$, we partition action space into non-resonant regions, single-resonance channels $\mathcal{R}_k$, double-resonance junctions $\mathcal{R}_{k^{(1)}} \cap \mathcal{R}_{k^{(2)}}$, up to $(n-1)$-fold resonances. Inside each block of multiplicity $r$, normal-form averaging eliminates all harmonics outside the $r$-dimensional resonant sublattice $\Lambda$, leaving an integrable resonant normal form plus an exponentially small remainder of size $\sim \exp(-(\epsilon_0/\epsilon)^a)$!
3. **Why Quasi-Convexity Blocks Fast Drift Along $\Lambda$:**
   Inside the resonant normal form of multiplicity $r$, the fast action drift $\dot{I}$ lies strictly in the $r$-dimensional subspace $\operatorname{span}(\Lambda)$ while conserving the unperturbed energy $H_0(I)$ up to $\mathcal{O}(\epsilon)$. Because $H_0(I)$ is strictly convex, the intersection of the affine plane $I(0) + \operatorname{span}(\Lambda)$ with the convex energy shell $\big|H_0(I) - H_0(I(0))\big| \le C\epsilon$ is a **small closed ellipsoid of diameter $\mathcal{O}(\epsilon^b)$**!
   Therefore, a trajectory can escape from its $\mathcal{O}(\epsilon^b)$ resonant trap *only* by climbing out under the exponentially small normal-form remainder $\sim \exp(-(\epsilon_0/\epsilon)^a)$, which requires at least $T_{\text{Nekh}}(\epsilon) \sim \exp((\epsilon_0/\epsilon)^a)$ time!

> **Counterexample when Convexity Fails:** If $H_0(I) = \frac{1}{2}(I_1^2 - I_2^2 + I_3^2)$ has an **indefinite Hessian** (not quasi-convex!), then along the null cone $I_1 = \pm I_2$, the resonant vector $k = (1, 1, 0)$ lies *inside* the energy surface ($k^T D^2 H_0 k = 0$), allowing fast $\mathcal{O}(\epsilon)$ drift without exponential suppression!

#### Illuminating the Arnold Web on the GPU: The Fast Lyapunov Indicator (FLI)

Because Arnold diffusion takes $\sim \exp((\epsilon_0/\epsilon)^a)$ steps to move $(I_1, I_2, I_3)$ macroscopically, how can a 60 Hz WebGL2 shader visualize the 3D Arnold web in real time? By computing the **Fast Lyapunov Indicator (FLI)** introduced by Claude Froeschlé, Elena Lega, and Rudolphe Gonczi (1997, *Science* 2000)!

At each point $I_0 = (I_1, I_2, I_3)$ in 3D action space (with fixed initial angles $\theta_0$), we evolve the symplectic map $z_{m+1} = M(z_m)$ together with a unit tangent vector $v_0 \in T_{z_0} M$ ($\|v_0\| = 1$) under the exact linearized Jacobian map $v_{m+1} = DM(z_m)\,v_m$ for a short horizon of $N$ steps ($N \approx 20\text{–}50$), and record the supremum of the logarithmic stretch:

$$\boxed{\; \operatorname{FLI}(I_0; N) := \sup_{1 \le m \le N} \ln\,\|v_m\| \;}$$

Look at how $\operatorname{FLI}(I_0; N)$ acts as an X-ray of the Arnold Web:
- **Diophantine KAM Tori:** Tangent vectors shear only linearly in time ($\|v_m\| \sim m\,\|D^2 H_0\|$), so $\operatorname{FLI} \approx \ln N$.
- **Centers of Resonant Islands:** Librating orbits have periodic tangent matrices, so $\operatorname{FLI}$ dips *below* the KAM background!
- **Whiskered Tori & Separatrices of the Arnold Web:** Along the hyperbolic borders of every resonant channel $k_1 \omega_1 + k_2 \omega_2 + k_3 \omega_3 = 0$ and especially at resonance junctions $\mathcal{R}_{k^{(1)}} \cap \mathcal{R}_{k^{(2)}}$, $\|v_m\| \sim e^{\lambda m}$ grows exponentially, making the resonant filaments glow in 3D action space!

---

## Lab Instructions (`module_06_lab.html`)

Open [`module_06_lab.html`](file:///Users/osf/pprojects/midi-sink/site/drafts/lessons/module_06/module_06_lab.html) in a WebGL2 + WebAudio browser. This capstone lab combines a **3D Raymarching FLI Shader over Action Space $(I_1, I_2, I_3)$** with a **High-Speed ($10^5$ steps/frame) Arnold-Web Drift Triadic Synthesizer**.

### Part 1: Shader Lab — 3D Volumetric Raymarching of the Arnold Web via Fast Lyapunov Indicators (FLI)

The top WebGL2 canvas raymarches a 3D volume of action space $(I_1, I_2, I_3) \in [0.15, 1.85]^3$ (drag with your mouse to orbit the 3D camera in azimuth and elevation). Along each camera ray, the fragment shader evaluates the local **Fast Lyapunov Indicator (FLI)** and multi-order resonance proximity for the coupled 3-DoF Froeschlé / Arnold map:

$$\begin{aligned}
I_i^{(m+1)} &= I_i^{(m)} + \epsilon\,\frac{\partial V(\theta^{(m)})}{\partial \theta_i}, \qquad \theta_i^{(m+1)} = \theta_i^{(m)} + 2\pi I_i^{(m+1)} \qquad (i = 1, 2, 3) \\
V(\theta_1, \theta_2, \theta_3) &= \cos\theta_1 + \cos\theta_2 + \cos\theta_3 + \mu\big(\cos(\theta_1 - \theta_2) + \cos(\theta_2 - \theta_3) + \cos(\theta_1 + \theta_2 - 2\theta_3)\big)
\end{aligned}$$

1. **Explore the 3D Arnold Web & Energy Shell $H_0(I) = E$:**
   - Start in **3D Volumetric Raymarched Arnold Web (FLI Filaments)** mode and drag the canvas to rotate the 3D action cube $(I_1, I_2, I_3)$.
   - Observe the glowing gold and cyan resonant planes $\sum_{i=1}^3 k_i I_i = 0$ intersecting the spherical isoenergetic shell $I_1^2 + I_2^2 + I_3^2 = 2E$ along great-circle channels, and notice the bright **Resonance Hubs (Junctions)** where three or more resonant sheets cross!
   - Switch to **Spherical Energy Surface Slice ($H_0(I) = E$) + FLI Web** or **2D Cross-Sectional FLI Tomography ($I_3 = \text{slice}$)** to inspect individual resonant channels ($1:1:0$, $1:2:-1$, $2:-1:-1$, $3:-2:0$, etc.) and their double-walled whiskered separatrix borders!
   - Sweep the **Perturbation $\epsilon$** slider from `0.005` (Nekhoroshev regime: razor-thin, isolated filaments) to `0.080` (Chirikov overlap regime: thickened, merging chaotic channels).

### Part 2: Sound Lab — Hearing Arnold Diffusion Along the Web ($10^5$ Steps per Audio Frame)

Because true Arnold diffusion along a transition chain is exponentially slow ($\sim \exp(-(\epsilon_0/\epsilon)^{1/6})$), running 1 step per audio sample would take hours to move an inch along the web. Instead, Part 2 runs a **High-Speed Symplectic Burst Engine executing up to $100,000$ ($10^5$) map steps per frame** along the 3D Arnold web while mapping the instantaneous 3D action vector $(I_1(t), I_2(t), I_3(t))$ to a **Three-Voice Musical Triad**:

$$f_1(t) = f_{\text{base}}\,I_1(t), \qquad f_2(t) = f_{\text{base}}\,I_2(t), \qquad f_3(t) = f_{\text{base}}\,I_3(t)$$

1. **Hear the Triad Migrate Along a Single Resonance Line & Pivot at a Junction:**
   - Click **Start 3-Voice Arnold Triad Synth**.
   - Watch the glowing trajectory marker on the **2D Projected Arnold Web Map** (and in the 3D Raymarcher) while listening to the three-voice chord $(f_1, f_2, f_3)$:
   - While the trajectory travels along a single resonance channel $\mathcal{R}_k$ (for instance, $\mathbf{k} = (2, -3, 1)$, so $2 f_1(t) - 3 f_2(t) + f_3(t) = 0$), all three pitches $f_1(t), f_2(t), f_3(t)$ **glide slowly and continuously across the spectrum**, yet they maintain the **exact linear harmonic interval constraint $k_1 f_1 + k_2 f_2 + k_3 f_3 = 0$**!
   - The moment the drifting action vector reaches a **Resonance Junction (Hub)** $\mathcal{R}_{k^{(1)}} \cap \mathcal{R}_{k^{(2)}}$ (such as $(I_1 : I_2 : I_3) = (3 : 4 : 6)$ or $(2 : 3 : 4)$), heteroclinic whisker scattering **pivots the trajectory onto a new intersecting resonance channel** $\mathcal{R}_{k^{(2)}}$, changing the interval rule of the gliding triad!
   - Use the **Burst Acceleration ($10^2 \dots 10^5$ steps/frame)** slider and the **Nekhoroshev $\epsilon$** slider to verify Nekhoroshev's exponential scaling law $T_{\text{Nekh}} \sim \exp((\epsilon_0/\epsilon)^{1/6})$ live in the telemetry bar!

---

## Problem Set

### Problem 6.1: Codimension of KAM Tori & Longitudinal Drift Vector on the Arnold Web

1. **Codimension & Separation in $n$ Degrees of Freedom:**
   For an autonomous $n$-DoF Hamiltonian system on $(M^{2n}, \omega, H)$, prove that if $H(q, p) = E$ is a regular energy value, then $\dim \Sigma_E = 2n - 1$, any Lagrangian KAM torus $\mathcal{T}_\omega \subset \Sigma_E$ has codimension $n - 1$ inside $\Sigma_E$, and $n = 2$ is the unique integer $n \ge 2$ for which $\operatorname{codim}_{\Sigma_E}(\mathcal{T}_\omega) = 1$.
   - For `libsumi`'s $N$-vortex Kirchhoff Hamiltonian $H_{\text{Kirchhoff}}(x_1, \dots, x_N)$ (`_work/TO_PROJECT_SPEC.md §10.8.2`), where the conserved linear and angular impulses reduce the effective degrees of freedom to $n_{\text{eff}} = N - 2$, determine the minimum number of vortices $N$ required for invariant tori to have codimension $\ge 2$ inside the reduced energy-impulse manifold.

2. **Isoenergetic Tangent Direction of a Resonance Channel ($n = 3$):**
   Let $H_0(I_1, I_2, I_3)$ be a smooth integrable Hamiltonian on $\mathbb{R}^3$ with frequency vector $\omega(I) = \nabla H_0(I)$ and Hessian matrix $Q(I) = D^2 H_0(I)$. Suppose $I^*$ lies on a single resonance surface $\mathcal{R}_k = \{I : k \cdot \omega(I) = 0\}$ for $k \in \mathbb{Z}^3 \setminus \{0\}$.
   - Show that the normal vector to the energy surface $\Sigma_E = \{H_0(I) = E\}$ at $I^*$ is $\omega(I^*)$, and the normal vector to the resonance surface $\mathcal{R}_k$ at $I^*$ is $Q(I^*)\,k$.
   - Conclude that the 1-dimensional **Arnold drift channel** $\mathcal{C}_k = \mathcal{R}_k \cap \Sigma_E$ has tangent vector:
     $$\mathbf{t}_k(I^*) = \omega(I^*) \times \big(Q(I^*)\,k\big)$$
   - Show that if $H_0(I)$ is strictly convex ($Q(I^*) \succ 0$), then the fast single-harmonic resonant oscillation vector $k$ is **never** tangent to $\mathcal{R}_k$ ($k \cdot (Q(I^*) k) > 0$), proving that fast $\mathcal{O}(\sqrt{\epsilon})$ motion can only oscillate *across* the width of $\mathcal{R}_k$, while any $\mathcal{O}(1)$ drift *along* $\mathbf{t}_k(I^*)$ requires multi-harmonic coupling!

---

### Problem 6.2: Whiskered Tori & Melnikov Splitting in Arnold's 1964 Model

Consider Arnold's 1964 Hamiltonian on $(I_1, I_2, \theta_1, \theta_2, t) \in \mathbb{R}^2 \times \mathbb{T}^2 \times S^1$:

$$H(I_1, I_2, \theta_1, \theta_2, t) = \frac{1}{2}\big(I_1^2 + I_2^2\big) + \epsilon\big(\cos\theta_1 - 1\big)\Big(1 + \mu\big(\sin\theta_2 + \cos t\big)\Big)$$

1. **Invariance of the Whiskered Tori for All $\mu$:**
   Compute Hamilton's equations for $\dot{I}_1, \dot{\theta}_1, \dot{I}_2, \dot{\theta}_2$, and show that for **every** $c \in \mathbb{R}$ and **every** $\epsilon, \mu \in \mathbb{R}$, the 2-torus
   $$\mathcal{T}_c := \big\{ (I_1, I_2, \theta_1, \theta_2, t) : I_1 = 0,\; \theta_1 = 0 \pmod{2\pi},\; I_2 = c \big\}$$
   is an **exact invariant torus** of the full flow with $\dot{\theta}_2 = c, \dot{t} = 1$! Why does the perturbation $\epsilon\mu(\cos\theta_1 - 1)(\sin\theta_2 + \cos t)$ vanish along with all its first derivatives on $\mathcal{T}_c$?

2. **Unperturbed Whisker Separatrix & First-Order Action Jump $\Delta I_2$:**
   When $\mu = 0$ and $\epsilon > 0$, the unstable and stable whiskers of $\mathcal{T}_c$ coincide along the pendulum separatrix:
   $$\theta_1^0(t - t_0) = 4\arctan\!\big(e^{\sqrt{\epsilon}(t - t_0)}\big), \qquad I_1^0(t - t_0) = \frac{2\sqrt{\epsilon}}{\cosh\!\big(\sqrt{\epsilon}(t - t_0)\big)}, \qquad \theta_2^0(t) = \theta_{2,0} + c\,t$$
   Using $\dot{I}_2 = -\frac{\partial H}{\partial \theta_2} = -\epsilon\mu\big(\cos\theta_1 - 1\big)\cos\theta_2$ and $\cos\theta_1^0(\tau) - 1 = -\frac{2}{\cosh^2(\sqrt{\epsilon}\tau)}$, integrate $\dot{I}_2$ along the unperturbed separatrix from $t = -\infty$ to $t = +\infty$ to show that to first order in $\mu$, a trajectory traversing the whisker experiences a net longitudinal action jump:
   $$\Delta I_2(\theta_{2,0}) = \int_{-\infty}^{\infty} \frac{2\epsilon\mu\,\cos(\theta_{2,0} + c t)}{\cosh^2(\sqrt{\epsilon} t)}\,dt = \frac{2\pi \mu c}{\sinh\!\left(\dfrac{\pi c}{2\sqrt{\epsilon}}\right)}\cos(\theta_{2,0})$$
   Verify that for $0 < \sqrt{\epsilon} \ll c$, the maximum action jump per whisker transit scales exponentially as:
   $$\max_{\theta_{2,0}} |\Delta I_2| \approx 4\pi \mu c\,\exp\!\left(-\frac{\pi c}{2\sqrt{\epsilon}}\right)$$

---

### Problem 6.3: Nekhoroshev's Stability Exponents & Variational Equations for the Fast Lyapunov Indicator (FLI)

1. **Optimal Ultraviolet Cutoff in Nekhoroshev Theory:**
   Suppose an analytic perturbation $H_1(I, \theta) = \sum_{k \in \mathbb{Z}^n} \hat{H}_{1,k}(I) e^{i k \cdot \theta}$ has Fourier coefficients decaying as $|\hat{H}_{1,k}| \le C e^{-\rho \|k\|_1}$. By Dirichlet's Box Principle (or Lochak's periodic-orbit approximation), within a ball of radius $R \sim \epsilon^b$ around any point $I(0) \in \mathbb{R}^n$, rational frequency vectors of period $T$ occur with spacing $\sim T^{-1/(n-1)}$. Balancing the resonant normal-form domain radius $\epsilon^b \sim (\epsilon K_{\text{UV}})^{-1}$ against the Fourier truncation remainder $\epsilon e^{-\rho K_{\text{UV}}}$ leads to $K_{\text{UV}} \sim \epsilon^{-a}$ with $a = b = \frac{1}{2n}$.
   - For $n = 3$ degrees of freedom ($a = 1/6$), if reducing $\epsilon$ by a factor of $64$ ($2^6$) increases $(\epsilon_0/\epsilon)^{1/6}$ by a factor of $2$, by what factor does Nekhoroshev's stability time $T_{\text{Nekh}}(\epsilon) = T_0 \exp((\epsilon_0/\epsilon)^{1/6})$ increase when $(\epsilon_0/\epsilon)^{1/6}$ goes from $5$ to $10$?

2. **Exact Variational Map for the 3-DoF Fast Lyapunov Indicator (FLI):**
   For the 3-DoF symplectic map $I_i' = I_i + \epsilon \frac{\partial V(\theta)}{\partial \theta_i}$, $\theta_i' = \theta_i + I_i'$ ($i = 1, 2, 3$), write the $6 \times 6$ tangent Jacobian matrix $DM(\theta, I)$ acting on a tangent vector $v = (\delta\theta, \delta I)^T \in \mathbb{R}^6$:
   $$\begin{pmatrix} \delta\theta' \\ \delta I' \end{pmatrix} = \begin{pmatrix} I_3 + \epsilon\,D^2 V(\theta) & I_3 \\ \epsilon\,D^2 V(\theta) & I_3 \end{pmatrix} \begin{pmatrix} \delta\theta \\ \delta I \end{pmatrix}$$
   Prove that $(DM)^T \Omega_6 (DM) = \Omega_6$ whenever $D^2 V(\theta)$ is symmetric (so the tangent map is itself symplectic with $\det DM = 1$!), and explain why $\operatorname{FLI}(N) = \sup_{1 \le m \le N} \ln\|v_m\|$ grows as $\ln N$ on regular KAM tori ($D^2 V \to 0$ in action-angle coordinates) but grows linearly $\propto \lambda N$ along hyperbolic whiskered tori.

