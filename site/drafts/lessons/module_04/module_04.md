# Module 4: Integrable Systems, Action-Angle Coordinates & KAM Theory

*Target codebase: `midi-sink` as shipped (`libsumi 1.12.0, Voxo 0.26.0`, 2026-10-25). All code citations and gate numbers refer to `PROJECT_SPEC.md`, `_work/TO_PROJECT_SPEC.md`, `_work/DECISIONS_13.md`, `voxo/src/suzu.h`, and `site/src/content/docs/operators/`.*

---

## Lecture Notes

### 4.1 Complete Integrability ($n$ Commuting Integrals) & The Liouville–Arnold Theorem

In Module 3 (§3.4), we saw that any 1-DoF Hamiltonian $H(q, p)$ on a 2-dimensional phase space $(M^2, \omega)$ with compact regular energy curves $H(q, p) = E$ is automatically integrable: the energy $H$ itself is the single required conserved quantity, each connected level set is a 1-torus $\mathbb{T}^1 \cong S^1$, and the enclosed phase-space area $I(E) = \frac{1}{2\pi}\oint_{\gamma_E} p\,dq$ gives canonical action-angle coordinates $(I, \theta)$.

What happens in higher dimensions $\dim M = 2n$ ($n \ge 2$), such as a 2D stadium billiard ($2n = 4$), a double pendulum ($2n = 4$), a 3-vortex Kirchhoff chord braid ($2n = 6$), or an $N$-node modal/Toda ring ($2n = 2N$)? Conservation of energy $H(q, p) = E$ restricts the trajectory only to a $(2n - 1)$-dimensional hypersurface. To confine trajectories to $n$-dimensional tori $\mathbb{T}^n$, we need **$n$ independent conservation laws that commute with one another**:

> **Definition 4.1 (Liouville Complete Integrability):** A Hamiltonian system on a $2n$-dimensional symplectic manifold $(M^{2n}, \omega, H)$ is **completely integrable** (in the sense of Liouville) if there exist $n$ smooth functions $F_1, F_2, \dots, F_n \in C^\infty(M)$ (with $F_1 = H$) such that:
> 1. **Involution (Mutual Commutativity):** Every pair of integrals has vanishing Poisson bracket:
>    $$\{F_i, F_j\} = \omega(X_{F_i}, X_{F_j}) = 0 \qquad \forall i, j \in \{1, \dots, n\}$$
> 2. **Functional Independence:** The differentials $dF_1, \dots, dF_n$ are linearly independent ($dF_1 \wedge \cdots \wedge dF_n \neq 0$) on a dense open subset of $M$.

#### Why Commuting Integrals Force Invariant Lagrangian Tori $\mathbb{T}^n$

Suppose $c = (c_1, \dots, c_n) \in \mathbb{R}^n$ is a regular value of the **momentum map** $\mathbf{F} = (F_1, \dots, F_n) : M^{2n} \to \mathbb{R}^n$, and consider the joint level set:

$$M_c := \mathbf{F}^{-1}(c) = \big\{ z \in M : F_1(z) = c_1, \dots, F_n(z) = c_n \big\}$$

By the Regular Value Theorem, $M_c$ is a smooth submanifold of dimension $2n - n = n$. Now examine the $n$ Hamiltonian vector fields $X_{F_1}, \dots, X_{F_n}$ along $M_c$:
- **Tangency to $M_c$:** The derivative of $F_j$ along $X_{F_i}$ is $dF_j(X_{F_i}) = \{F_j, F_i\} = 0$. Therefore, every $X_{F_i}$ is tangent to $M_c$, and since $dF_1, \dots, dF_n$ are linearly independent and $\omega$ is non-degenerate, $\big\{X_{F_1}(z), \dots, X_{F_n}(z)\big\}$ forms a basis for the tangent space $T_z M_c$ at every point $z \in M_c$!
- **$M_c$ is a Lagrangian Submanifold:** Evaluating the symplectic 2-form $\omega$ on any two basis vectors of $T_z M_c$ gives:
  $$\omega\big(X_{F_i}(z), X_{F_j}(z)\big) = \{F_i, F_j\}(z) = 0 \qquad \Longrightarrow \qquad \omega\big|_{TM_c} \equiv 0$$
  A half-dimensional submanifold ($\dim M_c = n = \frac{1}{2}\dim M$) on which $\omega$ vanishes identically is called a **Lagrangian submanifold**. Notice that $n$ is the *maximum* possible number of independent commuting integrals on $M^{2n}$, because the skew-orthogonal complement of an $n$-dimensional isotropic subspace has dimension $2n - n = n$.
- **Commuting Flows & Torus Topology:** Recall from Module 2 (§2.2) that the Lie bracket of two Hamiltonian vector fields satisfies $[X_{F_i}, X_{F_j}] = -X_{\{F_i, F_j\}} = 0$. Because the $n$ vector fields commute pairwise and are complete on a compact level set $M_c$, their flows $\Phi_{F_1}^{t_1} \circ \cdots \circ \Phi_{F_n}^{t_n}$ define a smooth, locally free, transitive group action of the additive group $(\mathbb{R}^n, +)$ on each connected component of $M_c$. The quotient of $\mathbb{R}^n$ by a discrete lattice $\Lambda_c \cong \mathbb{Z}^n$ of periods is an **$n$-torus** $\mathbb{T}^n = \mathbb{R}^n / 2\pi\mathbb{Z}^n$!

This geometric picture culminates in the **Liouville–Arnold Theorem (1855, 1963)**:

> **Theorem 4.2 (Liouville–Arnold):** Let $(M^{2n}, \omega, H)$ be a completely integrable system with momentum map $\mathbf{F} = (F_1, \dots, F_n)$. If a regular level set $M_c = \mathbf{F}^{-1}(c)$ is compact and connected, then:
> 1. $M_c$ is diffeomorphic to the $n$-torus $\mathbb{T}^n = (\mathbb{R}/2\pi\mathbb{Z})^n$.
> 2. In an open neighbourhood $U \supset M_c$ foliated by invariant Lagrangian tori, there exist canonical Darboux **action-angle coordinates** $(I_1, \dots, I_n, \theta_1, \dots, \theta_n) \in D \times \mathbb{T}^n$ defined by the cycle integrals of the tautological 1-form $\theta_{\text{taut}} = \sum_i p_i\,dq_i$ around $n$ independent homology cycles $\gamma_1(c), \dots, \gamma_n(c)$ of $H_1(M_c; \mathbb{Z}) \cong \mathbb{Z}^n$:
>    $$I_k(c) := \frac{1}{2\pi}\oint_{\gamma_k(c)} \sum_{i=1}^n p_i\,dq_i, \qquad \omega = \sum_{k=1}^n dI_k \wedge d\theta_k$$
>    (Why is $I_k(c)$ independent of how we deform the loop $\gamma_k$ on $M_c$? Because $M_c$ is Lagrangian: between two homologous loops $\gamma_k, \gamma_k' \subset M_c$ bounding a surface $\Sigma \subset M_c$, Stokes' theorem gives $\oint_{\gamma_k} p\,dq - \oint_{\gamma_k'} p\,dq = -\iint_\Sigma \omega|_{TM_c} = 0$!)
> 3. In action-angle coordinates, $F_1, \dots, F_n$ depend **only on the actions $I = (I_1, \dots, I_n)$**. In particular, $H = H_0(I_1, \dots, I_n)$, and Hamilton's equations linearize completely:
>    $$\dot{I}_k = -\frac{\partial H_0}{\partial \theta_k} = 0, \qquad \dot{\theta}_k = \frac{\partial H_0(I)}{\partial I_k} =: \omega_k(I) \qquad \Longrightarrow \qquad \theta_k(t) = \theta_k(0) + \omega_k(I)\,t \pmod{2\pi}$$

#### Integrable Tori Across `midi-sink` (`Suzu` & `libsumi`)

1. **The Circle vs. Bunimovich Stadium Billiard (`suzu::Billiard`, `voxo/src/suzu.h` lines 812–890):**
   In `suzu::Billiard`, a particle moves in 2D $(x_1, x_2)$ with conjugate velocity $(y_1, y_2)$ ($2n = 4$) under the harmonic spring $H = \frac{1}{2}(x_1^2 + x_2^2 + y_1^2 + y_2^2)$ inside a stadium wall of aspect $a/b$ and straight half-length $L = \text{hl}$ (`g(x_1, x_2) = 0`).
   - When $L = 0$ and $a = b$ (**Circular Billiard**), the wall is radially symmetric. The angular momentum $F_2 = L_z = x_1 y_2 - x_2 y_1$ is conserved across every specular reflection ($v' = v - 2(v \cdot \hat{n})\hat{n}$ because $\hat{n} \parallel x$) and commutes with $F_1 = H$: $\{H, L_z\} = 0$! With $n = 2$ commuting integrals on a 4D phase space, the circular billiard is **Liouville integrable**: every trajectory is confined to an invariant 2-torus $\mathbb{T}^2$, and in `tests/suzu_test.cpp` (`_work/DECISIONS_13.md #3`), the Poincaré collision strobe $(s, \sin\psi)$ stays locked on a 1D curve occupying only **24 / 576** histogram cells over 66,393 collisions.
   - The instant you raise the mod wheel ($L > 0$, the **Bunimovich Stadium**), the straight wall segments break rotational symmetry: $\{H, L_z\} \neq 0$ at wall reflections, the invariant 2-tori shatter via defocusing hyperbolicity, and a single trajectory ergodically sprays across **153 / 576** strobe cells!
2. **The Integrable $N$-Node Toda Lattice (`suzu::TodaRing`, `voxo/src/suzu.h` lines 700–767):**
   Why does a nonlinear exponential lattice $H(q, p) = \sum_{i=0}^{N-1} \frac{1}{2}p_i^2 + \frac{a}{b}\sum_{i=0}^{N-1}(e^{-b(q_{i+1}-q_i)} + b(q_{i+1}-q_i) - 1)$ never thermalize or break into chaos like a generic Fermi–Pasta–Ulam–Tsingou (FPUT) chain? Because Hénon and Flaschka (1974) proved that `TodaRing` possesses **$N$ independent global integrals in involution**:
   $$F_k(q, p) = \frac{1}{k}\operatorname{tr}\!\big(L(q, p)^k\big), \qquad \{F_j, F_k\} = 0 \quad (j, k = 1, \dots, N)$$
   where $L$ is the $N \times N$ periodic Jacobi Lax matrix with $b_i = -\frac{1}{2}b p_i$ on the diagonal and $a_i = \frac{1}{2}b\sqrt{a}\,e^{-b r_i / 2}$ on the off-diagonals! In action-angle coordinates $(I_1, \dots, I_N, \theta_1, \dots, \theta_N)$, multi-soliton collisions are simply linear phase shifts on an $N$-torus $\mathbb{T}^N$.
3. **Uncoupled Modal Banks (`suzu::Cell[MODES_MAX]`, `voxo/src/suzu.h` line 319):**
   A bank of $N \le 16$ modal resonators with frequencies $\omega_k(I) = \omega_k$ has $N$ commuting actions $I_k = \frac{1}{2}(x_k^2 + y_k^2 - \varepsilon_k x_k y_k)$. When the modes have irrational frequency ratios (such as the zeros of Bessel functions $J_m$ on a circular membrane or the roots of $\cos\beta\cosh\beta = 1$ on a free bar), the flow $\theta_k(t) = \theta_k(0) + \omega_k t$ winds densely around the $N$-torus $\mathbb{T}^N$, producing a crystalline, non-repeating inharmonic timbre.

---

### 4.2 Perturbed Integrable Systems, Small Divisors & The KAM Theorem

Almost no real physical system is exactly integrable: strings couple across a bridge (`lattice_kick` in `voxo/src/suzu.h` line 344), planets tug on one another, and nonlinear oscillators experience periodic drives (`suzu::Rotor` in `voxo/src/suzu.h` line 1368). Written in the action-angle coordinates $(I, \theta) \in D \times \mathbb{T}^n$ of an unperturbed integrable system $H_0(I)$, a perturbed Hamiltonian takes the form:

$$H(I, \theta) = H_0(I) + \epsilon\,H_1(I, \theta), \qquad |\epsilon| \ll 1$$

with perturbed equations of motion:

$$\dot{I}_k = -\epsilon\,\frac{\partial H_1(I, \theta)}{\partial \theta_k}, \qquad \dot{\theta}_k = \omega_k(I) + \epsilon\,\frac{\partial H_1(I, \theta)}{\partial I_k}, \qquad \omega(I) := \nabla_I H_0(I)$$

In 1892, Henri Poincaré called the study of $H(I, \theta) = H_0(I) + \epsilon H_1(I, \theta)$ the *"fundamental problem of dynamics."* For decades before 1954, physicists assumed (following Boltzmann's **ergodic hypothesis**) that *any* generic perturbation $\epsilon H_1(I, \theta) \neq 0$ would immediately destroy all invariant tori $\mathbb{T}^n_I$ and make the motion ergodic on the $(2n - 1)$-dimensional energy surface.

#### Classical Canonical Perturbation Theory & The Small Divisor Crisis

Why did classical perturbation series diverge? Let us try to straighten the perturbed system back into integrable form $\tilde{H}(J) = H_0(J) + \epsilon \langle H_1 \rangle_\theta(J) + \mathcal{O}(\epsilon^2)$ by applying a near-identity **Type-2 Generating Function** (Module 3, §3.3):

$$F_2(\theta, J) = \theta \cdot J + \epsilon\,S_1(\theta, J)$$

which generates the canonical transformation:

$$I = \frac{\partial F_2}{\partial \theta} = J + \epsilon\,\frac{\partial S_1(\theta, J)}{\partial \theta}, \qquad \Theta = \frac{\partial F_2}{\partial J} = \theta + \epsilon\,\frac{\partial S_1(\theta, J)}{\partial J}$$

Taylor-expanding $H(I, \theta) = H_0\!\big(J + \epsilon\frac{\partial S_1}{\partial \theta}\big) + \epsilon H_1(J, \theta)$ to first order in $\epsilon$ yields:

$$H(I, \theta) = H_0(J) + \epsilon\left[ \omega(J) \cdot \frac{\partial S_1(\theta, J)}{\partial \theta} + H_1(J, \theta) \right] + \mathcal{O}(\epsilon^2)$$

To eliminate the $\theta$-dependence at $\mathcal{O}(\epsilon)$, $S_1(\theta, J)$ must solve the **homological partial differential equation** on $\mathbb{T}^n$:

$$\omega(J) \cdot \frac{\partial S_1(\theta, J)}{\partial \theta} = \langle H_1 \rangle_\theta(J) - H_1(J, \theta)$$

Expanding $H_1(J, \theta) = \sum_{k \in \mathbb{Z}^n} \hat{H}_{1,k}(J)\,e^{i k \cdot \theta}$ and $S_1(\theta, J) = \sum_{k \in \mathbb{Z}^n \setminus \{0\}} \hat{S}_{1,k}(J)\,e^{i k \cdot \theta}$ in Fourier series on the $n$-torus $\mathbb{T}^n$, we can solve for each Fourier coefficient $\hat{S}_{1,k}(J)$ algebraically:

$$\boxed{\; \hat{S}_{1,k}(J) = \frac{i\,\hat{H}_{1,k}(J)}{k \cdot \omega(J)} = \frac{i\,\hat{H}_{1,k}(J)}{k_1 \omega_1(J) + \cdots + k_n \omega_n(J)} \qquad \big(k \in \mathbb{Z}^n \setminus \{0\}\big) \;}$$

Look at the denominator $k \cdot \omega(J)$:
1. **Exact Rational Resonance ($k \cdot \omega(J) = 0$):** Whenever the frequencies $\omega_1(J), \dots, \omega_n(J)$ are rationally commensurate ($k_1 \omega_1 + \cdots + k_n \omega_n = 0$ for some integer vector $k \in \mathbb{Z}^n \setminus \{0\}$), the denominator **vanishes** and $\hat{S}_{1,k}(J)$ blows up!
2. **Dense Small Divisors:** Even if $\omega(J)$ is irrational ($k \cdot \omega(J) \neq 0$ for all $k \neq 0$), integer combinations $k \cdot \omega(J)$ become **arbitrarily close to zero** as $\|k\|_1 = |k_1| + \cdots + |k_n| \to \infty$! Furthermore, if $H_0(I)$ is **non-degenerate** ($\det \frac{\partial \omega}{\partial I} = \det \frac{\partial^2 H_0}{\partial I^2} \neq 0$, meaning the frequencies vary with action $I$), then the resonant actions $\{J : k \cdot \omega(J) = 0\}$ form a **dense subset** of action space $D$!

#### The Kolmogorov–Arnold–Moser (KAM) Theorem (1954, 1962, 1963)

In 1954, Andrey Kolmogorov announced (and Vladimir Arnold and Jürgen Moser proved in 1962–1963) a stunning resolution to the small divisor problem: **most invariant tori do not break!**

How does KAM overcome the small divisors $k \cdot \omega$? By combining two insights:
1. **Diophantine Irrationality Condition:** Instead of trying to save *all* tori, fix a frequency vector $\omega \in \mathbb{R}^n$ that is poorly approximated by rationals — a **Diophantine vector** satisfying, for some constants $\gamma > 0$ and $\tau > n - 1$:
   $$\boxed{\; |k \cdot \omega| \ge \frac{\gamma}{\|k\|_1^\tau} \qquad \forall k \in \mathbb{Z}^n \setminus \{0\} \;}$$
   When $H_1$ is real-analytic (or sufficiently smooth), its Fourier coefficients decay exponentially: $|\hat{H}_{1,k}| \le C e^{-\rho \|k\|_1}$. On a Diophantine torus, the polynomial growth $\|k\|_1^\tau / \gamma$ of the small divisor $1/|k \cdot \omega|$ is completely overwhelmed by the exponential decay $e^{-\rho \|k\|_1}$ of the numerator, so the solution $S_1$ of the homological equation converges!
2. **Kolmogorov's Superconvergent Newton Iteration (Quadratic Convergence $\epsilon \to \epsilon^2 \to \epsilon^4 \to \epsilon^8 \dots$):** Because the frequency $\omega(J)$ shifts slightly at each perturbative step, a standard power series in $\epsilon$ still diverges. Kolmogorov instead adjusted the action $J$ at each step to keep the frequency vector locked to the fixed Diophantine $\omega$ (requiring the **twist / non-degeneracy condition** $\det \frac{\partial^2 H_0}{\partial I^2} \neq 0$) and applied a **Newton–Kantorovich iteration** in canonical transformations, squaring the error at step $m$ ($\epsilon_{m+1} \sim \epsilon_m^2$) so fast that it outruns the shrinking analyticity strip!

> **Theorem 4.3 (KAM Theorem):** Let $H(I, \theta) = H_0(I) + \epsilon H_1(I, \theta)$ be analytic (or $C^\ell$ with $\ell > 2\tau + 2$, Moser 1962) on $D \times \mathbb{T}^n$, and suppose $H_0$ is non-degenerate:
> $$\det \left(\frac{\partial \omega_i}{\partial I_j}\right) = \det \left(\frac{\partial^2 H_0(I)}{\partial I_i \partial I_j}\right) \neq 0 \quad \text{on } D$$
> Then for $|\epsilon|$ sufficiently small, every unperturbed torus $\mathbb{T}^n_{I_0}$ whose frequency vector $\omega = \nabla H_0(I_0)$ satisfies the Diophantine condition with $\gamma \sim \sqrt{|\epsilon|}$ **persists** as a smoothly deformed invariant Lagrangian torus $\mathcal{T}_\omega \subset D \times \mathbb{T}^n$ of the perturbed flow, on which the motion is canonically conjugate to the linear quasi-periodic translation $\dot{\Theta} = \omega$.
> Moreover, the union of surviving KAM tori is a **fat Cantor set of positive Lebesgue measure** whose complement (the resonant gaps and chaotic layers) has measure $\mathcal{O}(\sqrt{|\epsilon|}) \to 0$ as $\epsilon \to 0$.

---

### 4.3 Poincaré Sections, Rotation Numbers, Poincaré–Birkhoff Islands & Arnol'd Lock

How do we visualize a 4D continuous Hamiltonian flow (like the **Hénon–Heiles** galactic/molecular potential $H = \frac{1}{2}(p_x^2 + p_y^2) + \frac{1}{2}(x^2 + y^2) + x^2 y - \frac{1}{3}y^3$ or Suzu's `Billiard`) or a 2D time-periodic system on a 2D GPU canvas?

#### Poincaré Surface of Section & Empirical Rotation (Winding) Numbers

Given a 2-DoF autonomous Hamiltonian $H(q_1, q_2, p_1, p_2) = E$ on a 3D energy hypersurface $\Sigma_E \subset \mathbb{R}^4$, we slice $\Sigma_E$ with a 2D transverse plane — for instance, $q_1 = 0$ with $p_1 > 0$ — and record the 2D coordinates $(q_2, p_2)$ every time the trajectory pierces the plane. By Liouville's Theorem / Poincaré's recurrence construction, the resulting **Poincaré return map** $P : (q_2, p_2) \mapsto (q_2', p_2')$ is an exact **area-preserving symplectomorphism** ($\det DP = 1$)!

Similarly, for a periodically kicked rotor on the cylinder $(\theta, p) \in \mathbb{T}^1 \times \mathbb{R}$, sampling once per kick period gives the **Chirikov Standard Map** (`site/src/content/docs/operators/chirikov.mdx`, `suzu::Rotor` in `voxo/src/suzu.h` line 1368):

$$\boxed{\; p_{n+1} = p_n + K\sin(\theta_n), \qquad \theta_{n+1} = \theta_n + p_{n+1} \pmod{2\pi} \;}$$

How does a GPU shader distinguish a surviving **KAM torus**, a **resonant Poincaré–Birkhoff island chain**, and a **chaotic sea** on a $1024 \times 1024$ grid of initial conditions $(\theta_0, p_0)$? By computing each pixel's **empirical rotation (winding) number** $\omega(\theta_0, p_0)$ over $N$ iterations (using the un-wrapped angle $\tilde{\theta}_N$):

$$\omega(\theta_0, p_0) := \lim_{N \to \infty} \frac{\tilde{\theta}_N - \tilde{\theta}_0}{2\pi N} = \lim_{N \to \infty} \frac{1}{2\pi N}\sum_{n=1}^N p_n$$

Look at the three distinct geometric signatures of $\omega(\theta_0, p_0)$:
1. **Diophantine KAM Curve ($\omega$ irrational):** The orbit $(\theta_n, p_n)$ traces a smooth, unbroken 1D curve wrapping all the way around the cylinder $\theta \in [0, 2\pi)$, and $\omega_N$ converges rapidly ($\mathcal{O}(1/N)$, or $\mathcal{O}(1/N^2)$ with Birkhoff weighting) to a constant irrational number!
   - **Which KAM curve is the last to break?** By Hurwitz's theorem on Diophantine approximation, a real number's continued fraction $\omega = [a_0; a_1, a_2, a_3, \dots] = a_0 + \frac{1}{a_1 + \frac{1}{a_2 + \cdots}}$ is *hardest* to approximate by rationals when every partial quotient is as small as possible: $a_k = 1$ for all $k \ge 1$! That gives the **noble golden-ratio winding numbers**:
     $$\omega_{\text{gold}} = \varphi^{-1} = \frac{\sqrt{5} - 1}{2} = [0; 1, 1, 1, \dots] \approx 0.6180339887, \qquad 1 - \omega_{\text{gold}} = \varphi^{-2} \approx 0.3819660113$$
   - Using **Greene's Residue Method (1979)** on the Fibonacci rational periodic orbits $p_m / q_m = F_{m-1}/F_m$ ($1/2, 2/3, 3/5, 5/8, 8/13, 13/21 \dots$), John Greene proved that the golden KAM curve survives up to the universal critical kick strength:
     $$\boxed{\; K_c \approx 0.971635406 \;}$$
   - In 2D phase space, a 1D closed curve divides the cylinder into two disconnected halves. Therefore, for **$K < K_c$**, the golden KAM curves act as **impenetrable topological dams**: no chaotic trajectory can ever cross them, and momentum $p_n$ is strictly bounded forever! For **$K > K_c$**, the golden KAM curve shatters into a Cantor dust (**cantorus**) with turnstile gaps through which trajectories leak across the entire cylinder (`suzu::Rotor` transitions from sideband-modulated tone to pitched noise across the octave!).

2. **Resonant Torus Breakup & The Poincaré–Birkhoff Theorem ($\omega = r / s \in \mathbb{Q}$):**
   What happens at an unperturbed rational winding number $\omega = r / s$ ($p_0 = 2\pi r / s$), where every point of the unperturbed circle $\theta_{n+s} = \theta_n + 2\pi r$ is a fixed point of the $s$-fold iterate $M^s$?
   When $K > 0$, the **Poincaré–Birkhoff Theorem** proves that the continuous circle of period-$s$ points cannot survive intact; instead, at least **$2s$ periodic points** survive in an alternating chain around the cylinder:
   - **$s$ stable elliptic periodic points** ($\lvert\operatorname{tr}(DM^s)\rvert < 2$), each surrounded by a vortex-like **resonant island** of librating trajectories whose rotation number is **locked flat at $\omega \equiv r / s$** across the entire finite width of the island! Plotting $\omega$ as a function of initial momentum $p_0$ produces a fractal ** Devil's Staircase** of flat rational plateaus at $0/1, 1/1, 1/2, 1/3, 2/3, 2/5, 3/5 \dots$
   - **$s$ unstable hyperbolic saddle points** ($\lvert\operatorname{tr}(DM^s)\rvert > 2$), whose stable and unstable manifolds $W^s, W^u$ split apart and intersect transversely infinitely many times (Poincaré's **homoclinic tangle**), forming a chaotic stochastic layer framing the island chain!

3. **How `libsumi`'s Arnol'd Lock Operator (`arnold.mdx`) Draws Musical Intervals as Poincaré–Birkhoff Islands:**
   Look at [`site/src/content/docs/operators/arnold.mdx`](file:///Users/osf/pprojects/midi-sink/site/src/content/docs/operators/arnold.mdx). In Kessho mode (`press_mode = 3`), holding a chord locks every upper voice to the lowest held bass note! The engine reads the frequency ratio $f / f_{\text{bass}}$ (including MPE pitch bend), takes its fractional part $\Omega_0$, and finds the nearest continued-fraction rational $p/q$ ($2 \le q \le 6$) inside the **Arnol'd tongue** $|\Omega_0 - p/q| < \frac{1}{2q^2}$.
   Every frame, `arnold_fs` applies one iteration of the canonical kicked twist map $M = T \circ \text{Kick}$ in Darboux polar coordinates $(\rho = r^2/2, \theta)$ generated by the Type-2 generating function:
   $$F_2(\theta, \rho_1) = \theta\rho_1 + \frac{K}{q}W(\rho_1)\cos q(\theta - \varphi) \quad \Longrightarrow \quad \begin{cases} \rho_1 = \rho + K\,W(\rho_1)\sin q(\theta - \varphi) \\ \theta_1 = \theta + \frac{K}{q}W'(\rho_1)\cos q(\theta - \varphi) \end{cases}$$
   followed by the Lamb–Oseen twist $T$ rotating the ring by $\Omega_0$ turns per frame (inverted in the shader by 5 scalar Newton steps on $\theta$, keeping $\det J = 1$ identically):
   - **Just Intonation = Phase-Locked Poincaré–Birkhoff Islands:** A pure $3:2$ fifth ($\Omega_0 = 1/2, q = 2$) or $5:4$ major third ($\Omega_0 = 1/4, q = 4$) sits at the exact rational centre of the tongue: the $q$ Poincaré–Birkhoff islands **stand still on the water** over 600+ frames while ink librates inside them!
   - **Equal Temperament = Stroboscopic Island Precession:** An equal-tempered MIDI fifth ($2^{7/12} \approx 1.4983$, 2 cents flat of $3:2$) has $\delta = \Omega_0 - 1/2 \neq 0$, so its 2 islands precess once every 600 frames; an equal-tempered major third ($2^{4/12}$, 14 cents sharp of $5:4$) precesses $6\times$ faster!

---

### 4.4 Polyphonic Torus Sonification at 48 kHz: Diophantine Chords vs. Resonant Phase-Locking

In Part 2 of this module's lab (and inside `Suzu`'s coupled modal lattice `lattice_kick`, `voxo/src/suzu.h` lines 321–354, as well as `suzu::Rotor` line 1368), we turn the geometry of an $n$-torus $\mathbb{T}^n$ directly into polyphonic sound at 48 kHz.

Consider an $n$-dimensional torus with angles $\theta(t) = (\theta_1(t), \dots, \theta_n(t)) \in \mathbb{T}^n$ and conjugate actions $I(t) = (I_1(t), \dots, I_n(t))$ evolving under the perturbed Hamiltonian:

$$H(I, \theta) = \underbrace{\sum_{k=1}^n \left(\omega_{0,k} I_k + \frac{1}{2}\alpha_k I_k^2\right)}_{H_0(I)} + \epsilon \underbrace{\sum_{m} V_m(I) \cos\big(k^{(m)} \cdot \theta\big)}_{H_1(I, \theta)}$$

where each torus coordinate drives an audible partial $\sin(\theta_k(t))$ with instantaneous frequency:

$$\dot{\theta}_k(t) = \frac{\partial H}{\partial I_k} = \omega_{0,k} + \alpha_k I_k(t) + \epsilon\frac{\partial H_1}{\partial I_k}, \qquad \dot{I}_k(t) = -\frac{\partial H}{\partial \theta_k} = \epsilon \sum_m k^{(m)}_k V_m(I) \sin\big(k^{(m)} \cdot \theta\big)$$

Listen to what happens in three distinct dynamical regimes as you move the initial condition $(I_0, \theta_0)$ across phase space or sweep the perturbation $\epsilon$:

1. **On a Diophantine KAM Torus (e.g., Golden-Ratio / Metallic-Mean Ratios $\omega_2/\omega_1 = \varphi^{-1}$):**
   Because $|k \cdot \omega| \ge \gamma \|k\|_1^{-\tau}$ is bounded away from zero, the resonant angle combinations $k^{(m)} \cdot \theta(t)$ rotate rapidly and never lock. The actions $I_k(t)$ undergo only tiny, bounded $\mathcal{O}(\epsilon)$ ripples around $I_k(0)$, and the instantaneous frequencies $\dot{\theta}_k(t)$ stay locked to their Diophantine ratio. Audibly, the polyphonic sum $\sum_{k=1}^n \sqrt{2I_k}\sin(\theta_k(t))$ is a **stationary, crystalline, shimmer-free inharmonic chord** whose Lissajous projection on $\mathbb{T}^n$ weaves an unbroken ergodic carpet!
2. **Inside a Broken Rational Resonance / Poincaré–Birkhoff Island ($k_1 \omega_1 + k_2 \omega_2 \approx 0$, e.g., $2:3$, $3:4$, or $4:5$):**
   Define the **resonant slow phase** $\psi(t) := k_1 \theta_1(t) - k_2 \theta_2(t)$ and its conjugate resonant action $J(t)$. Expanding $H(I, \theta)$ near the resonance surface $k_1 \omega_1(I^*) - k_2 \omega_2(I^*) = 0$ reduces the slow $(\psi, J)$ dynamics to the **pendulum Hamiltonian** of Module 2:
   $$H_{\text{res}}(J, \psi) \approx \frac{1}{2}\Lambda J^2 - \epsilon V_{k}\,\cos(\psi), \qquad \ddot{\psi} + \Omega_{\text{lib}}^2 \sin(\psi) = 0, \qquad \Omega_{\text{lib}} = \sqrt{|\epsilon \Lambda V_k|}$$
   Inside the resonant island ($H_{\text{res}} < |\epsilon V_k|$), the resonant phase $\psi(t)$ **cannot circulate**; instead, it **librates** back and forth around $0$ at the low **island libration frequency** $\Omega_{\text{lib}} \propto \sqrt{\epsilon}$!
   What does that sound like at 48 kHz?
   - **Rhythmic Phase-Locking:** The ratio of the average frequencies $\langle \dot{\theta}_1 \rangle / \langle \dot{\theta}_2 \rangle$ is **pinned exactly to the rational ratio $k_2 / k_1$** (an Arnol'd tongue!).
   - **Low-Frequency FM & AM Sidebands:** Because $J(t)$ and $\dot{\theta}_k(t) = \omega_k(I^*) \pm k_k \Lambda J(t)$ oscillate back and forth at the slow libration frequency $\Omega_{\text{lib}} \propto \sqrt{\epsilon}$, the voices frequency-modulate and exchange action energy $I_1 \leftrightarrow I_2$ rhythmically at $\Omega_{\text{lib}}$! You hear unmistakable **pulsing FM sidebands** ($\omega_k \pm m\Omega_{\text{lib}}$) and slow acoustic beating inside the locked interval!
3. **Across the Separatrix / Above Greene's Threshold ($K > K_c \approx 0.9716$):**
   Placing the cursor right on the hyperbolic saddle / homoclinic tangle of a broken resonance (or pushing $K > 0.9716$) sends $\psi(t)$ tumbling erratically between libration and rotation, causing the instantaneous frequencies $\dot{\theta}_k(t)$ to wander chaotically — the exact acoustic signature of `suzu::Rotor` (`voxo/src/suzu.h` line 1365) crossing Greene's threshold!

---

## Lab Instructions (`module_04_lab.html`)

Open [`module_04_lab.html`](file:///Users/osf/pprojects/midi-sink/site/drafts/lessons/module_04/module_04_lab.html) in a WebGL2 + WebAudio browser. The two workspaces are directly linked: clicking any point on the GPU Poincaré / Winding-Number Shader Map in Part 1 immediately sets the initial condition $(\theta_0, I_0)$ of the 48 kHz Polyphonic Torus Synthesizer in Part 2!

### Part 1: Shader Lab — GPU Poincaré Maps, Winding Numbers & KAM / Poincaré–Birkhoff Structure

Each pixel in the WebGL2 shader represents an initial condition $(q_0, p_0)$ for the **Chirikov Standard Map** (or **Arnol'd Tongue Circle-Twist Map** `arnold_fs` / **Stroboscopic Hénon–Heiles Map**). For every pixel in parallel, the fragment shader iterates the symplectic map up to **500 steps per frame** and computes the empirical rotation number:

$$\omega(q_0, p_0) = \frac{\tilde{\theta}_N - \tilde{\theta}_0}{2\pi N}$$

1. **Locate Golden-Ratio KAM Curves vs. Poincaré–Birkhoff Island Chains:**
   - Start in **High-Contrast Winding Number + Golden KAM & Rational Resonance Highlight** mode at $K = 0.75$ (below Greene's threshold $K_c \approx 0.9716$).
   - Observe how the shader highlights:
     - **Bright Gold Contours:** The surviving **Golden-Ratio Diophantine KAM Tori** at $\omega = \varphi^{-1} \approx 0.61803$ and $\omega = 1 - \varphi^{-1} \approx 0.38197$, stretching unbroken all the way across $q \in [0, 2\pi)$ as topological barriers!
     - **Cyan / Magenta Island Chains:** The **Poincaré–Birkhoff Resonant Islands** at $\omega = 1/2, 1/3, 2/3, 1/4, 3/4, 2/5, 3/5$, where the rotation number $\omega$ locks onto a flat constant plateau across each elliptic eye!
   - Sweep the **Perturbation / Kick Strength $K$** slider across **Greene's Threshold $K_c = 0.9716$**:
     - At $K = 0.50$, invariant KAM curves foliate almost the entire phase space.
     - At $K = 0.9716$ (click the **Snap $K_c = 0.9716$ (Greene)** button), watch the golden-ratio curve wrinkle critically between the growing $1/2$, $2/3$, and $3/5$ Fibonacci island chains.
     - Above $K = 1.25$, the golden barrier breaks and the chaotic sea floods vertically across the cylinder!

2. **Inspect the Devil's Staircase Slice & Single-Orbit Strobe Overlay:**
   - Move or click your cursor anywhere on the WebGL2 map. The live **CPU Orbit Strobe & Devil's Staircase Overlay** traces the 500-point trajectory $(q_n, p_n)$ of your selected initial condition right on top of the shader and plots the 1D **Devil's Staircase** curve $\omega(p_0)$ along the vertical cross-section!

### Part 2: Sound Lab — 48 kHz Polyphonic $n$-Torus Sonification

The bottom 48 kHz WebAudio engine integrates a **3-Voice Coupled Torus System** $(I_1, I_2, I_3, \theta_1, \theta_2, \theta_3) \in D \times \mathbb{T}^3$ driven by the cursor's action ratio $I_0$ (or selected via one-click presets):

1. **Hear a Diophantine KAM Torus vs. a Broken Rational Resonance:**
   - Click **Start 48 kHz Torus Synth** and click **Preset 1: Golden Diophantine KAM Torus ($\omega_2/\omega_1 = \varphi^{-1} \approx 0.6180$)**.
   - Listen to the polyphonic output $\sum_{k=1}^3 A_k \sin(\theta_k(t))$: even with coupling $\epsilon > 0$, the frequencies $\dot{\theta}_k(t)$ stay locked to their Diophantine ratio with zero phase-locking and zero low-frequency beating — a pure, crystalline inharmonic bell chord! Look at the **Torus Lissajous $(\sin\theta_1, \sin\theta_2)$ Scope** (filling a smooth invariant annulus) and the **Instantaneous Frequency $\dot{\theta}_k(t)$ Strip Chart** (three steady horizontal tracks).
   - Now click **Preset 2: Broken $2:3$ Poincaré–Birkhoff Resonance Island** (or click directly inside the $\omega = 1/2$ or $\omega = 2/3$ island eye on the Part 1 shader canvas!).
   - Immediately hear the chord **phase-lock into the rational resonance** while the slow pendulum libration $\ddot{\psi} + \Omega_{\text{lib}}^2\sin\psi = 0$ inside the island produces **rhythmic low-frequency FM sidebands and action-exchange pulsing** between the partials!
   - Now click **Preset 3: Homoclinic Separatrix / Chaotic Sea ($K > K_c$)** to hear the resonant phase $\psi(t)$ slip erratically across the hyperbolic saddle, turning the stationary chord into a shimmering chaotic texture.

---

## Problem Set

### Problem 4.1: Complete Integrability of the Circular Billiard & The 2-Body Toda Ring

1. **Commuting Integrals on the Circular Billiard (`suzu::Billiard`, `voxo/src/suzu.h` lines 812–890):**
   Consider the 2-DoF harmonic oscillator in the plane $(x_1, x_2, y_1, y_2) \in \mathbb{R}^4$ with symplectic form $\omega = dx_1 \wedge dy_1 + dx_2 \wedge dy_2$, Hamiltonian $F_1 = H = \frac{1}{2}(x_1^2 + x_2^2 + y_1^2 + y_2^2)$, and angular momentum $F_2 = L_z = x_1 y_2 - x_2 y_1$.
   - Compute the canonical Poisson bracket $\{H, L_z\}$ directly and verify $\{H, L_z\} = 0$.
   - Show that at a specular reflection $y' = y - 2(y \cdot \hat{n})\hat{n}$ off the boundary wall $g(x_1, x_2) = 0$, $L_z$ is conserved **if and only if** the wall normal $\hat{n}(x_1, x_2)$ is everywhere parallel to the position vector $(x_1, x_2)$ (i.e., the circular wall $L = 0, a = b$). Explain why setting $L > 0$ (`hl > 0` in `suzu::Billiard`) breaks $L_z$-conservation on the straight segments and destroys the invariant 2-tori.

2. **Lax Pair & Involution for the Toda Ring (`suzu::TodaRing`, `voxo/src/suzu.h` lines 700–767):**
   For the 2-node periodic Toda ring ($N = 2$, $r_0 = q_1 - q_0$, $r_1 = q_0 - q_1 = -r_0$), the Hamiltonian is:
   $$H(q_0, q_1, p_0, p_1) = \frac{1}{2}(p_0^2 + p_1^2) + \frac{a}{b}\left(e^{-b(q_1 - q_0)} + e^{b(q_1 - q_0)} - 2\right)$$
   Identify the two commuting integrals $F_1 = H$ and $F_2 = P_{\text{tot}} = p_0 + p_1$, verify $\{H, P_{\text{tot}}\} = 0$, and construct the canonical transformation to center-of-mass and relative coordinates $(Q_{\text{cm}}, Q_{\text{rel}}, P_{\text{cm}}, P_{\text{rel}})$ that separates the 4D phase space into two independent 1-DoF systems.

---

### Problem 4.2: Small Divisors, Diophantine Numbers & Greene's Golden KAM Threshold

Consider a 2-torus perturbation $H(I_1, I_2, \theta_1, \theta_2) = H_0(I_1, I_2) + \epsilon H_1(\theta_1, \theta_2)$ where $\omega_1(I) = 1$ and $\omega_2(I) = I_2$ (so the twist matrix is $\frac{\partial \omega}{\partial I} = \begin{pmatrix} 0 & 0 \\ 0 & 1 \end{pmatrix}$ on the isoenergetic slice).

1. **The Homological Equation & Measure of Resonant Zones:**
   For each integer pair $(k_1, k_2) \in \mathbb{Z}^2 \setminus \{(0,0)\}$ with $k_2 > 0$, the resonant ratio $-k_1 / k_2$ is surrounded by a Diophantine exclusion zone of width:
   $$\left|\omega_2 - \frac{-k_1}{k_2}\right| < \frac{\gamma}{(|k_1| + |k_2|)^\tau}$$
   Show that for $\omega_2 \in [0, 1]$ and any exponent $\tau > 2$, the total Lebesgue measure of the union of all exclusion zones over all $(k_1, k_2) \in \mathbb{Z}^2 \setminus \{(0,0)\}$ is bounded by $C_\tau \gamma$, where $C_\tau = \sum_{k_2 = 1}^\infty k_2 \cdot \frac{2}{k_2^\tau} = 2\zeta(\tau - 1) < \infty$. Conclude that as $\gamma \to 0$, the set of Diophantine numbers in $[0, 1]$ has full Lebesgue measure $1$ despite having an empty interior!

2. **Why the Golden Ratio $\varphi^{-1} = \frac{\sqrt{5}-1}{2}$ Survives Longest (`chirikov.mdx`, `suzu::Rotor`):**
   Show that $\omega_* = \frac{\sqrt{5}-1}{2}$ satisfies $\omega_* = \frac{1}{1 + \omega_*}$, so its continued-fraction expansion is $[0; 1, 1, 1, \dots]$ with rational convergents given by the ratios of consecutive Fibonacci numbers $F_{m-1}/F_m$. Verify that for all integers $p, q$ with $q > 0$:
   $$\left|\omega_* - \frac{p}{q}\right| \ge \frac{1}{(\sqrt{5} + o(1))\,q^2}$$
   which is the largest possible asymptotic constant $\gamma$ allowed by Hurwitz's theorem ($\gamma \le 1/\sqrt{5}$).

---

### Problem 4.3: Arnol'd Tongue Generating Function & Pendulum Reduction at Resonance (`arnold.mdx`)

1. **Bijectivity Bound of `arnold_fs` (`site/src/content/docs/operators/arnold.mdx` lines 21–36):**
   In Kessho's Arnol'd Lock operator, the kick in polar action-angle coordinates $(\rho = \frac{1}{2}r^2, \theta)$ is generated by the Type-2 generating function:
   $$F_2(\theta, \rho_1) = \theta\rho_1 + \frac{K}{q}W(\rho_1)\cos q(\theta - \varphi)$$
   - Derive the canonical relations $\rho = \frac{\partial F_2}{\partial \theta}$ and $\theta_1 = \frac{\partial F_2}{\partial \rho_1}$, and verify that they reproduce equations (23)–(24) of `arnold.mdx`:
     $$\rho_1 = \rho + K\,W(\rho_1)\sin q(\theta - \varphi), \qquad \theta_1 = \theta + \frac{K}{q}W'(\rho_1)\cos q(\theta - \varphi)$$
   - Show that for fixed $\rho_1$, the angle map $\theta \mapsto \theta_1(\theta)$ has derivative $\frac{\partial \theta_1}{\partial \theta} = 1 - K\,W'(\rho_1)\sin q(\theta - \varphi)$, so the map is strictly monotonic and globally invertible on $S^1$ whenever $K < \frac{1}{\max|W'(\rho_1)|}$ (explaining the shipped engine's safety bound $K_{\max} = 0.8 / \max|W'|$ at line 34 of `arnold.mdx`!).

2. **Libration Frequency Inside a Resonant Island:**
   Near a $p : q$ resonance ($\Omega_0 = p/q$) of the Arnol'd map at action $\rho = \rho_0$ where $W(\rho_0) = 1$ and the Lamb–Oseen twist has shear $\Omega'(\rho_0) = \Lambda \neq 0$, linearize the $q$-step deviation $\delta\rho = \rho - \rho_0$ and resonant phase $\psi = q(\theta - \varphi)$ to show that the island's elliptic fixed point sits at $\sin\psi = 0$ and small oscillations inside the island librate with frequency $\Omega_{\text{lib}} = \sqrt{q\,K\,|\Lambda|}$ per $q$-frame cycle.

