---
title: "Module 3 — Darboux's theorem, Moser's trick and generating functions"
description: "Riemannian strain against symplectic flatness inside Kessho, Darboux coordinates on the crystal drop, Moser's homotopy, the four types of generating function and the engine's uses of them: the Arnol'd kick, the Chirikov map, the sheared cell and its calibration, the retune."
---
*Lecture notes written against `midi-sink` as shipped (libsumi 1.12.0, Voxo 0.26.0, 2026-10-25) and checked claim by claim against the engine before this fold (2026-10-26; libsumi is unchanged since, and Voxo's later versions add the sampler's second generation and touch no synth claim here). Code citations and gate numbers refer to `docs/PROJECT_SPEC.md` (spec v6), `docs/DECISIONS.md` Part XIII (cited `DECISIONS_13 #n`), `voxo/src/suzu.h`, and `site/src/content/docs/operators/`. The lab beside the module is a standalone teaching re-implementation, never the engine; the engine's own lab is [the Suzu lab](../../suzu/lab/).*

---

## Lecture Notes

### 3.1 Riemannian Curvature vs. Symplectic Flexibility: Darboux's Theorem

Why do two-dimensional fluid sheets, phase spaces, and canonical mechanical systems behave so differently from curved elastic surfaces? The answer lies in the fundamental contrast between **Riemannian geometry** and **symplectic geometry**:

| Geometric Structure | Defining Tensor on $TM$ | Algebraic Symmetry | Differential Condition | Local Equivalence (Normal Form) | Local Invariants |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Riemannian Manifold** $(M^m, g)$ | Metric $g = \sum g_{ij}\,dx^i \otimes dx^j$ | **Symmetric** ($g(u, v) = g(v, u)$), positive-definite | None (Levi-Civita connection $\nabla$ determined by $g$) | $g|_p = \delta_{ij}$ at a **single point** $p$ (geodesic normal coordinates: $g_{ij}(x) = \delta_{ij} - \frac{1}{3}R_{ikjl}x^k x^l + \mathcal{O}(\|x\|^3)$) | **Riemann curvature tensor** $R^i_{jkl}$ (in 2D: Gaussian curvature $\mathcal{K}(x,y)$) |
| **Symplectic Manifold** $(M^{2n}, \omega)$ | 2-Form $\omega = \frac{1}{2}\sum \omega_{ij}\,dz^i \wedge dz^j$ | **Skew-symmetric** ($\omega(u, v) = -\omega(v, u)$), non-degenerate | **Closed:** $d\omega = 0$ | $\omega = \sum_{i=1}^n dQ_i \wedge dP_i$ on an **entire open neighbourhood** $U \ni p$ (**Darboux's Theorem**) | **None!** (Only the dimension $2n$) |

In Riemannian geometry, Gauss's *Theorema Egregium* proves that the Gaussian curvature $\mathcal{K}$ of a 2D surface is a local isometric invariant: no smooth coordinate transformation $(x, y) \mapsto (u, v)$ can flatten a hemisphere ($\mathcal{K} = 1/R^2 > 0$) or a saddle ($\mathcal{K} < 0$) onto the Euclidean plane ($\mathcal{K} = 0$) without stretching distances.

In symplectic geometry, the situation is the exact opposite:

> **Theorem 3.1 (Darboux's Theorem, 1882):** Let $(M^{2n}, \omega)$ be a smooth $2n$-dimensional symplectic manifold ($d\omega = 0$ and $\omega^{\wedge n} \neq 0$). Around every point $p \in M$, there exists a local coordinate chart $(U;\, Q_1, \dots, Q_n, P_1, \dots, P_n)$ centered at $p$ in which $\omega$ takes the constant canonical form:
> $$\omega\big|_U = \omega_0 := \sum_{i=1}^n dQ_i \wedge dP_i$$
> Such coordinates $(Q, P)$ are called **canonical** or **Darboux coordinates**. Consequently, all symplectic manifolds of the same dimension $2n$ are locally indistinguishable.

#### How `libsumi`'s Kessho Medium Separates Riemannian Strain from Symplectic Flatness

You can see the contrast between Riemannian invariants and Darboux's Theorem directly inside `libsumi`'s **Kessho** crystal/interference medium (`composite.glsl`'s `kessho_col`, the spec draft's §10.7, [the Minerals page](../../operators/minerals/)).

After $N$ exact suminagashi passes (`drop_fs`, `vortex_fs`, `chladni_fs`, `tine_fs`), the displacement field — the ping-pong texture, RGBA16F by default — stores the cumulative Lagrangian displacement $d(x, y)$ mapping current canvas coordinates $x = (x, y)$ back to their initial origin $P_0(x) = x + d(x)$ with Jacobian matrix $J(x) = I + \nabla d(x)$, read by central differences over texels. From this single Jacobian $J(x)$, Kessho extracts **two completely different geometric pullbacks**:

1. **The Symplectic 2-Form Pullback (the `--kessho-flat` view at $z = 1$):**
   Pulling the canonical area 2-form $\omega_0 = dx \wedge dy$ back by $P_0$ gives:
   $$P_0^*\omega_0 = (\det J(x))\,dx \wedge dy$$
   Because every exact operator is a symplectomorphism ($\det J(x) = 1$), $P_0^*\omega_0 = dx \wedge dy = \omega_0$ everywhere! Symplectically, the wildly swirled suminagashi sheet is **completely flat** — in the Lagrangian coordinates $(Q, P) = P_0(x, y)$, the 2-form is still $dQ \wedge dP$.

2. **The Riemannian Metric Pullback (Layer A, the birefringence) and the Rotation Side (Layer B, the trace discriminant):**
   Pulling the Euclidean metric $g_0 = dx^2 + dy^2$ back by $P_0$ yields the symmetric **Cauchy–Green deformation tensor**:
   $$C(x) = J(x)^T J(x) = (I + \nabla d)^T (I + \nabla d)$$
   Even though $\det C = (\det J)^2 = 1$, the matrix $C(x)$ is **not** the identity! Its eigenvalues are $\sigma(x)^2 \ge 1$ and $\sigma(x)^{-2} \le 1$, where $\sigma(x) > 1$ is the principal stretch along the sheared ink filament and $\psi(x)$ is the principal strain angle. In Kessho (`minerals.mdx`), this Riemannian anisotropy drives **photoelastic birefringence**: light splits into fast and slow axes with optical path retardation $\Gamma \propto \Delta n\,(\sigma - \sigma^{-1})$, painting the Michel-Lévy interference fringes and the uniaxial Maltese cross $\sin^2 2(\psi - \alpha)$!
   Meanwhile, Layer B reads the *rotation* side of the same Jacobian, where the metric is blind. The **trace discriminant** $\Delta = (\operatorname{tr} J)^2 - 4$ sorts each texel as elliptic ($\Delta < 0$, a cool tint) or hyperbolic ($\Delta > 0$, a warm one): a pure rotation has $C = I$ and no birefringence at all, yet $\Delta = 4(\cos^2\phi - 1) < 0$ tints it. The caustic is the determinant of the sheet lifted to a viewing depth $z$ — for an area-preserving $J$, $\det(I + z\nabla d) = 1 + z(1 - z)(\operatorname{tr} J - 2)$ — so its fold is the iso-trace contour $\operatorname{tr} J = 2 + 1/(z(z - 1))$, read at three depths $z_R, z_G, z_B$ by Barite's dispersion. There is no heightfield and no curvature in the medium. Where $|\Delta| < 0.15$ and $\sigma > 0.05$ — near the parabolic line of $\Delta$, where the two eigenvalues of $J$ coincide — Barite's `conical_ring` reads the caustic through a ring kernel of eight points: **Hamilton's conical refraction**, a stylisation of the hollow cone on the fold (Module 1 §1.5 lists these observables with their thresholds).

---

### 3.2 Moser's Homotopy Trick ($\frac{d}{dt}\psi_t^*\omega_t = 0$)

How do we actually construct the coordinate transformation $\psi : (q, p) \mapsto (Q, P)$ that flattens a spatially varying closed 2-form $\omega_1$ into the constant canonical 2-form $\omega_0 = \sum_i dq_i \wedge dp_i$?

In 1965, Jürgen Moser introduced one of the most powerful tools in differential geometry — **Moser's Trick** (or the **homotopy method**). Instead of trying to solve a nonlinear partial differential equation for the static map $\psi$ directly, Moser builds a continuous family of diffeomorphisms $\psi_t$ ($t \in [0, 1]$) generated by a **time-dependent vector field $X_t$**!

#### Step-by-Step Derivation of Moser's Flow

Suppose $\omega_0$ and $\omega_1$ are two closed 2-forms ($d\omega_0 = d\omega_1 = 0$) on a star-shaped neighbourhood $U \subset \mathbb{R}^{2n}$ such that $\omega_1 - \omega_0$ is exact, and the convex linear interpolation

$$\omega_t := (1 - t)\omega_0 + t\omega_1 = \omega_0 + t(\omega_1 - \omega_0), \qquad t \in [0, 1]$$

is **non-degenerate** for every $t \in [0, 1]$. We seek a smooth 1-parameter family of maps $\psi_t : U \to U$ starting at the identity ($\psi_0 = \mathrm{id}$) such that:

$$\psi_t^*\omega_t = \omega_0 \qquad \forall t \in [0, 1]$$

If we can find such a family, then at $t = 1$ we have $\psi_1^*\omega_1 = \omega_0$, and $(Q, P) = \psi_1^{-1}(q, p)$ are our desired Darboux coordinates!

Let $X_t$ be the time-dependent velocity field generating $\psi_t$:

$$\frac{d}{dt}\psi_t(z) = X_t\big(\psi_t(z)\big), \qquad \psi_0(z) = z$$

Now differentiate $\psi_t^*\omega_t$ with respect to $t$. By the product rule for pullbacks and **Cartan's Magic Formula** ($\mathcal{L}_{X_t}\omega_t = d(i_{X_t}\omega_t) + i_{X_t}d\omega_t$):

$$\frac{d}{dt}\left(\psi_t^*\omega_t\right) = \psi_t^*\!\left( \mathcal{L}_{X_t}\omega_t + \frac{d\omega_t}{dt} \right) = \psi_t^*\!\Big( d(i_{X_t}\omega_t) + i_{X_t}(\underbrace{d\omega_t}_{=\,0}) + \omega_1 - \omega_0 \Big)$$

Because $d(\omega_1 - \omega_0) = d\omega_1 - d\omega_0 = 0$, the **Poincaré Lemma** guarantees the existence of a 1-form $\alpha$ on $U$ such that:

$$\omega_1 - \omega_0 = d\alpha$$

Substituting $\omega_1 - \omega_0 = d\alpha$ into the time derivative yields:

$$\frac{d}{dt}\left(\psi_t^*\omega_t\right) = \psi_t^*\!\Big( d\big(i_{X_t}\omega_t + \alpha\big) \Big)$$

Therefore, to guarantee $\frac{d}{dt}(\psi_t^*\omega_t) = 0$ identically for all $t \in [0, 1]$, it suffices to choose $X_t$ at each instant $t$ so that the 1-form inside $d(\cdots)$ vanishes:

$$\boxed{\; i_{X_t}\omega_t = -\alpha \;}$$

Look at how clean **Moser's Equation** ($i_{X_t}\omega_t = -\alpha$) is:
1. Because $\omega_t$ is **non-degenerate** for every $t \in [0, 1]$, the bundle map $v \mapsto i_v\omega_t$ is invertible at every point and every $t \in [0, 1]$. Thus $X_t = -(\omega_t^\flat)^{-1}(\alpha)$ is uniquely and explicitly determined by linear algebra!
2. If $\alpha$ vanishes at a point $p_0$ (or along a submanifold), then $X_t(p_0) = 0$ for all $t$, so $\psi_t(p_0) = p_0$ stays fixed while the surrounding space flows into canonical Darboux form.

#### Explicit 2D Formula for the Shader Lab ($\omega_1 = f(q, p)\,dq \wedge dp$)

On $U \subset \mathbb{R}^2$ (or on a $2\text{D}$ phase-space slice), let $\omega_0 = dq \wedge dp$ and let $\omega_1 = f(q, p)\,dq \wedge dp$ with $f(q, p) > 0$ strictly positive everywhere. Notice that:
- Because $f(q, p) > 0$, the homotopy density $f_t(q, p) := (1 - t) + t\,f(q, p) = 1 + t\big(f(q, p) - 1\big)$ is strictly positive for all $t \in [0, 1]$! Thus $\omega_t = f_t(q, p)\,dq \wedge dp$ never degenerates.
- Any 1-form $\alpha = \alpha_q(q, p)\,dq + \alpha_p(q, p)\,dp$ satisfying $d\alpha = \omega_1 - \omega_0$ obeys:
  $$d\alpha = \left(\frac{\partial \alpha_p}{\partial q} - \frac{\partial \alpha_q}{\partial p}\right) dq \wedge dp = \big(f(q, p) - 1\big)\,dq \wedge dp$$
  For instance, we can construct $\alpha$ by integrating along $q$ (the horizontal gauge $\alpha_q = 0, \alpha_p(q, p) = \int_0^q (f(s, p) - 1)\,ds$), or symmetrically along rays from the origin, or via a Poisson potential $\alpha = -\frac{\partial \phi}{\partial p}\,dq + \frac{\partial \phi}{\partial q}\,dp$ where $\Delta \phi = f(q, p) - 1$.
- Writing $X_t = X_t^q \frac{\partial}{\partial q} + X_t^p \frac{\partial}{\partial p}$, we compute the interior product with $\omega_t = f_t(q, p)\,dq \wedge dp$:
  $$i_{X_t}\omega_t = f_t(q, p)\,\big(X_t^q\,dp - X_t^p\,dq\big) = -\alpha_q\,dq - \alpha_p\,dp$$
  Equating coefficients of $dq$ and $dp$ gives the **explicit Moser Homotopy Vector Field**:
  $$\boxed{\; X_t(q, p) = \frac{1}{1 + t\big(f(q, p) - 1\big)} \begin{pmatrix} -\alpha_p(q, p) \\ \alpha_q(q, p) \end{pmatrix} \;}$$

Let us check what happens when we integrate $\frac{d}{dt}z(t) = X_t(z(t))$ from $t = 0$ to $t = 1$:
- Along any trajectory $z(t) = \psi_t(z_0)$, the Jacobian determinant $J_t(z_0) = \det D\psi_t(z_0)$ evolves by Liouville's trace formula $\frac{d}{dt}\ln J_t = (\nabla \cdot X_t) \circ \psi_t$.
- Because $\psi_t^*\omega_t = \omega_0$, we have $f_t(\psi_t(z_0)) \cdot \det D\psi_t(z_0) = 1$ for all $t \in [0, 1]$!
- At $t = 1$, $\det D\psi_1(z_0) = \frac{1}{f(\psi_1(z_0))}$: regions where $f > 1$ (dense phase-space metric) expand by $f$, while regions where $0 < f < 1$ (sparse phase-space metric) contract by $f$, morphing a warped coordinate grid into uniform canonical Darboux squares of equal $\omega_1$-area!

#### Hamilton's 1832 Crystal Drop (`drop_fs`, `drop.mdx`) as a Darboux Coordinate Map

Look at [the Drop page](../../operators/drop/) (the crystal drop's habits) and [the Minerals page](../../operators/minerals/). On the punctured plane $\mathbb{R}^2 \setminus \{C\}$, naive polar coordinates $(r, \theta)$ are **not** Darboux coordinates because the Euclidean area form has a spatially varying density $f(r, \theta) = r$:

$$\omega = dx \wedge dy = r\,dr \wedge d\theta$$

How do we flatten $\omega = r\,dr \wedge d\theta$ into canonical Darboux form $d\rho \wedge d\theta$? Applying Moser's radial integration $\rho(r) = \int_0^r s\,ds = \frac{1}{2}r^2$ gives **Hamilton's polar action-angle coordinates** $(\rho, \theta) = (\frac{1}{2}r^2, \theta)$:

$$d\rho \wedge d\theta = \left(r\,dr\right) \wedge d\theta = dx \wedge dy$$

In the Darboux chart $(\rho, \theta)$:
1. A circular ink drop of radius $R$ at centre $C$ is simply a **uniform translation** in $\rho$:
   $$\rho_{src} = \rho - \frac{1}{2}R^2, \quad \theta_{src} = \theta \qquad \Longleftrightarrow \qquad r_{src} = \sqrt{r^2 - R^2}$$
2. A **crystal habit drop** (`drop_habit`: calcite cleavage rhomb, quartz/muscovite hexagon, or barite desert-rose rosette $R(\theta) = r_0(1 + 0.6\cos^8 m\theta)/(1 + 0.6 \cdot 35/128)$, mixed toward the circle by the roundness) replaces the constant $R$ with any angular profile $R(\theta) > 0$:
   $$\rho_{src} = \rho - \frac{1}{2}R(\theta)^2, \qquad \theta_{src} = \theta$$
   In Darboux coordinates $(\rho, \theta)$, this map is a **pure triangular shear** with Jacobian:
   $$\frac{\partial(\rho_{src}, \theta_{src})}{\partial(\rho, \theta)} = \begin{pmatrix} 1 & -R(\theta)R'(\theta) \\ 0 & 1 \end{pmatrix} \quad \Longrightarrow \quad \det J = 1 \text{ identically!}$$
   Because $(\rho, \theta) \mapsto (x, y)$ is Darboux ($\det \frac{\partial(x,y)}{\partial(\rho,\theta)} = 1$), the composed map in Cartesian texels is **exact and area-preserving at any size and for any star-shaped crystal habit**, with zero sub-stepping!

---

### 3.3 Generating Functions of Canonical Transformations ($S(q, Q)$ & $F_2(q, P)$)

Darboux's Theorem tells us that canonical coordinates $(Q, P)$ always exist locally, and Moser's trick constructs them by flowing along $X_t$. Is there a direct, algebraic way to parameterize canonical transformations $\Phi : (q, p) \mapsto (Q, P)$ without integrating a differential equation at every step?

Recall from Module 1 (§1.2) that the canonical symplectic form is the negative exterior derivative of the **tautological (Liouville) 1-form**:

$$\theta_{\text{old}} = \sum_{i=1}^n p_i\,dq_i, \quad \omega_{\text{old}} = -d\theta_{\text{old}} = \sum_{i=1}^n dq_i \wedge dp_i, \qquad \theta_{\text{new}} = \sum_{i=1}^n P_i\,dQ_i, \quad \omega_{\text{new}} = -d\theta_{\text{new}} = \sum_{i=1}^n dQ_i \wedge dP_i$$

A diffeomorphism $\Phi : (q, p) \mapsto (Q, P)$ is a **canonical transformation (symplectomorphism)** if and only if $\Phi^*\omega_{\text{new}} = \omega_{\text{old}}$, which we can rewrite using $d\theta$:

$$\Phi^*\omega_{\text{new}} - \omega_{\text{old}} = -d\big(\Phi^*\theta_{\text{new}} - \theta_{\text{old}}\big) = d\left(\sum_{i=1}^n p_i\,dq_i - \sum_{i=1}^n P_i\,dQ_i\right) = 0$$

By the **Poincaré Lemma**, the 1-form $p\,dq - P\,dQ$ is closed ($d(p\,dq - P\,dQ) = 0$) on a simply connected domain if and only if it is the **exact differential of a scalar function** — called a **Generating Function**! Depending on which $n$ of the $2n$ coordinates $(q, p, Q, P)$ we choose as independent variables on the $2n$-dimensional graph of $\Phi$, Legendre transforms give the **Four Classical Types of Generating Functions**:

| Type | Independent Variables | Exact 1-Form Identity | Implicit / Explicit Canonical Equations | Non-Degeneracy (Twist) Condition |
| :--- | :--- | :--- | :--- | :--- |
| **Type 1** | $F_1 = S(q, Q)$<br>*(old & new positions)* | $p\,dq - P\,dQ = dS(q, Q)$ | $p_i = \dfrac{\partial S(q, Q)}{\partial q_i}, \quad P_i = -\dfrac{\partial S(q, Q)}{\partial Q_i}$ | $\det \left(\dfrac{\partial^2 S}{\partial q_i \partial Q_j}\right) \neq 0$ |
| **Type 2** | $F_2(q, P) = S + Q \cdot P$<br>*(old pos, new mom)* | $p\,dq + Q\,dP = dF_2(q, P)$ | $p_i = \dfrac{\partial F_2(q, P)}{\partial q_i}, \quad Q_i = \dfrac{\partial F_2(q, P)}{\partial P_i}$ | $\det \left(\dfrac{\partial^2 F_2}{\partial q_i \partial P_j}\right) \neq 0$ |
| **Type 3** | $F_3(p, Q) = S - q \cdot p$<br>*(old mom, new pos)* | $-q\,dp - P\,dQ = dF_3(p, Q)$ | $q_i = -\dfrac{\partial F_3(p, Q)}{\partial p_i}, \quad P_i = -\dfrac{\partial F_3(p, Q)}{\partial Q_i}$ | $\det \left(\dfrac{\partial^2 F_3}{\partial p_i \partial Q_j}\right) \neq 0$ |
| **Type 4** | $F_4(p, P) = S + Q \cdot P - q \cdot p$<br>*(old & new momenta)* | $-q\,dp + Q\,dP = dF_4(p, P)$ | $q_i = -\dfrac{\partial F_4(p, P)}{\partial p_i}, \quad Q_i = \dfrac{\partial F_4(p, P)}{\partial P_i}$ | $\det \left(\dfrac{\partial^2 F_4}{\partial p_i \partial P_j}\right) \neq 0$ |

Let us look at three concrete families of generating functions that power `midi-sink`:

1. **Type-1 Generating Functions $S(q, Q)$ & Twist Maps (`chirikov.mdx`, `suzu::Rotor`):**
   Choose the Type-1 generating function:
   $$S(q, Q) = \frac{1}{2\varepsilon}(Q - q)^2 + V(q)$$
   Applying $p = \frac{\partial S}{\partial q}$ and $P = -\frac{\partial S}{\partial Q}$ yields:
   $$p = -\frac{1}{\varepsilon}(Q - q) + V'(q), \qquad P = -\frac{1}{\varepsilon}(Q - q) \qquad \Longrightarrow \qquad \begin{cases} P = p - V'(q) \\ Q = q + \varepsilon P \end{cases}$$
   That is **Symplectic Euler (kick-then-drift)**! In particular, when $V(q) = \frac{K}{\varepsilon}\cos(q)$, $S(q, Q)$ generates the **Chirikov Standard Map** (`site/src/content/docs/operators/chirikov.mdx` and `suzu::Rotor` in `voxo/src/suzu.h`). Notice why $\det J = 1$ holds automatically for *any* potential $V(q)$: the map is generated by a single scalar function $S(q, Q)$ with twist $\frac{\partial^2 S}{\partial q \partial Q} = -\frac{1}{\varepsilon} \neq 0$!

2. **Type-1 Cross-Coupled Nonlinear Warps $S(q, Q) = q Q + W(q, Q)$ and Type-2 Point-Shear Warps $F_2(q, P)$:**
   - If we choose $F_2(q, P) = f(q)\,P + G(P)$ where $f : \mathbb{R} \to \mathbb{R}$ is any smooth invertible coordinate warp ($f'(q) > 0$), then:
     $$Q = \frac{\partial F_2}{\partial P} = f(q) + G'(P), \qquad p = \frac{\partial F_2}{\partial q} = f'(q)\,P$$
     which inverts explicitly in forward form from canonical oscillator coordinates $(Q, P)$ to warped physical coordinates $(q, p)$:
     $$\boxed{\; q = f^{-1}\!\big(Q - G'(P)\big), \qquad p = f'(q)\,P \;}$$
     Let us check the Jacobian of $(Q, P) \mapsto (q, p)$: it is the composition of the horizontal shear $\tilde{Q} = Q - G'(P)$ ($\det = 1$) followed by the canonical point-cotangent lift $q = f^{-1}(\tilde{Q})$, $p = f'(q)P$, whose Jacobian matrix is:
     $$\frac{\partial(q, p)}{\partial(\tilde{Q}, P)} = \begin{pmatrix} \dfrac{1}{f'(q)} & 0 \\[8pt] \dfrac{f''(q)}{f'(q)}P & f'(q) \end{pmatrix} \quad \Longrightarrow \quad \det \frac{\partial(q, p)}{\partial(Q, P)} = \frac{1}{f'(q)} \cdot f'(q) = \mathbf{1} \text{ identically!}$$
     Look at the physical meaning of $p = f'(q)P$: **whenever a coordinate transformation $q = f^{-1}(Q)$ compresses the position axis ($d q / d Q = 1/f'(q) < 1$), it must proportionally stretch the conjugate momentum axis ($p = f'(q) P$) so that phase-space area $dq \wedge dp = dQ \wedge dP$ remains invariant!**
   - Similarly, if we start from a Type-1 generating function $S(q, Q)$ with twist $\frac{\partial^2 S}{\partial q \partial Q} = 1 + \beta\,\phi'(q)\psi'(Q) > 0$, or compose a sequence of exact canonical shears generated by $S$:
     $$q = Q + g(P), \qquad p = P + h(q)$$
     we obtain an explicit, invertible canonical transformation $\Phi : (Q, P) \mapsto (q, p)$ that folds and warps phase space into complex multi-lobed filaments while keeping $\det D\Phi \equiv 1$.

---

### 3.4 Action-Angle Coordinates ($\oint p\,dq = 2\pi I$) & Canonical Timbre Warping at 48 kHz

Why does a canonical transformation $\Phi : (Q, P) \mapsto (q, p)$ allow us to warp an audio oscillator's **timbre** (harmonic overtone spectrum) as aggressively as we like **without ever detuning its fundamental pitch**?

#### The Action Integral $\oint p\,dq$ and Fundamental Frequency

For any 1-DoF Hamiltonian $H(q, p)$ whose level sets $H(q, p) = E$ are closed periodic orbits $\gamma_E = \partial D_E$, **Action-Angle Theory** (the 1D global version of Darboux's Theorem, known as the **Liouville–Arnold Theorem**) defines the **action variable** $I(E)$ as the enclosed symplectic area divided by $2\pi$:

$$I(E) := \frac{1}{2\pi}\oint_{\gamma_E} p\,dq = \frac{1}{2\pi}\iint_{D_E} dq \wedge dp$$

Together with the conjugate **angle variable** $\varphi \in [0, 2\pi)$ advancing uniformly in time, $(I, \varphi)$ form canonical Darboux coordinates ($\omega = dI \wedge d\varphi = dq \wedge dp$) in which $H = H(I)$ depends on the action $I$ alone! Hamilton's equations in $(I, \varphi)$ are simply:

$$\dot{I} = -\frac{\partial H}{\partial \varphi} = 0, \qquad \dot{\varphi} = \frac{\partial H(I)}{\partial I} =: \omega(I)$$

Thus the **exact fundamental angular frequency** of the orbit at energy $E$ is the derivative of energy with respect to enclosed phase-space area:

$$\boxed{\; \omega(E) = \frac{dE}{dI} = 2\pi \left(\frac{d}{dE}\iint_{D_E} dq \wedge dp\right)^{-1} \;}$$

Now compare two ways of adding nonlinear waveshaping to a harmonic oscillator $H_0(Q, P) = \frac{1}{2}\omega_0(Q^2 + P^2)$ (where $I = \frac{1}{2}(Q^2 + P^2) = E/\omega_0$, so $\omega(I) = \omega_0$ is completely independent of amplitude):

1. **Method A — Non-Canonical In-Loop Force Shear (`Cell::step_sheared` in `voxo/src/suzu.h`):**
   In `suzu.h`, `Cell::step_sheared(s_of_y)` modifies the restoring kick *inside* the recurrence:
   $$x_{n+1} = x_n - \varepsilon\big(y_n + s(y_n)\big), \qquad y_{n+1} = y_n + \varepsilon x_{n+1}$$
   Even though this single-step map has $\det J = 1$, it changes the **Hamiltonian itself** from $H_0(x, y) = \frac{1}{2}(x^2 + y^2)$ to $H_{\text{sheared}}(x, y) = \frac{1}{2}x^2 + \frac{1}{2}y^2 + \int_0^y s(u)\,du$. Because the level curve $H_{\text{sheared}}(x, y) = E$ encloses a **different area** $2\pi I_{\text{sheared}}(E) \neq 2\pi E/\omega_0$, the derivative $\omega(E) = dE / dI_{\text{sheared}}$ changes!
   As documented in `voxo/src/suzu.h`:
   - For the cubic hardening spring `shape_cubic(u) = u³`, the frequency shifts sharp by $\approx \sqrt{1 + \frac{3}{4}g}$ to first order, leaving **110 cents** of detuning at $g = 1$.
   - For the triangle wavefolder `shape_tri(u)`, the first-order describing function leaves a massive **555 cents** (nearly a tritone!) of detuning at $g = 1$.
   To keep `step_sheared` in tune across the keyboard, `suzu.h` must precompute a 17-point numerical period-calibration table (`shear_table_fill` / `shear_period_ratio`) by running a test cell for 80 cycles at $\varepsilon = 2\sin(\pi/256)$ and rescaling $\varepsilon$ by the measured period ratio!

2. **Method B — Canonical Coordinate Transformation via a Generating Function $\Phi : (Q, P) \mapsto (q, p)$:**
   Suppose instead that we evolve the canonical harmonic oscillator $(Q(t), P(t))$ under $H_0(Q, P) = \frac{1}{2}\omega_0(Q^2 + P^2)$ (or equivalently, evolve $(q, p)$ under the **pullback Hamiltonian** $H(q, p) := H_0(\Phi^{-1}(q, p))$), where $\Phi : (Q, P) \mapsto (q, p)$ is a canonical transformation generated by $S(q, Q)$ or $F_2(q, P)$:
   $$\Phi^*(dq \wedge dp) = dQ \wedge dP$$
   By Stokes' Theorem, for every closed orbit $\gamma_E$ in the $(Q, P)$ plane and its warped image $\tilde{\gamma}_E = \Phi(\gamma_E)$ in the $(q, p)$ plane, **the action integral is strictly preserved**:
   $$I_{\text{warped}}(E) = \frac{1}{2\pi}\oint_{\Phi(\gamma_E)} p\,dq = \frac{1}{2\pi}\iint_{\Phi(D_E)} dq \wedge dp = \frac{1}{2\pi}\iint_{D_E} \Phi^*(dq \wedge dp) = \frac{1}{2\pi}\iint_{D_E} dQ \wedge dP = I_0(E)$$
   Because $I_{\text{warped}}(E) \equiv I_0(E) = E / \omega_0$ for **every** energy $E$ and **every** generating function $S$, the fundamental frequency is:
   $$\omega_{\text{warped}}(E) = \frac{dE}{dI_{\text{warped}}} = \frac{dE}{dI_0} = \omega_0 \qquad (\mathbf{0.000\text{ cents detuning!}})$$
   Geometrically, as the canonical transformation $\Phi$ folds, pinches, or twists the circular $(Q, P)$ orbit into a figure-eight, star, or S-shaped wavefolder contour in $(q, p)$, **every lobe that stretches outward in $q$ is compensated by a proportional narrowing in $p$** so that the total enclosed area $\oint p\,dq$ never changes by a single bit! Thus:
   - The fundamental pitch remains **locked at $\omega_0$** with zero calibration tables;
   - The waveform $(q(t), p(t))$ acquires rich, dynamic harmonic overtones (wavefolding) as the generating function's warp depth increases;
   - Driving stereo speakers in quadrature ($L = q(t), R = p(t)$, as in Module 1) lets you hear both the nonlinear coordinate wavefolder on $q(t)$ and the conjugate momentum compression $p(t) = \frac{\partial S}{\partial q}$ that keeps the symplectic area constant!

#### Three More Places Where Darboux & Canonical Coordinate Maps Appear in `Suzu`

1. **MPE Pitch-Glide Re-Basing (`Cell::retune` vs. `Cell::retune_plain`, `voxo/src/suzu.h`):**
   In Module 2, we saw that the magic circle `Cell::step` conserves the tilted quadratic form $I_\varepsilon(x, y) = x^2 + y^2 - \varepsilon x y$. Writing $x = Q$ and $y = \frac{\varepsilon}{2}Q + \sqrt{1 - \frac{\varepsilon^2}{4}}\,P$ puts $I_\varepsilon$ into canonical Darboux form $I_\varepsilon = \big(1 - \frac{\varepsilon^2}{4}\big)(Q^2 + P^2)$, where the peak amplitude of $x$ is $A = \sqrt{I_\varepsilon / (1 - \varepsilon^2/4)}$.
   When an MPE performer slides pitch by $\pm 48$ semitones, $\varepsilon = 2\sin(\pi f / f_s)$ changes every sample. If you simply overwrite `eps = e` (`Cell::retune_plain`), the state $(x, y)$ lands on a new $\varepsilon$-ellipse with an amplitude that depends on the phase at the retune — $\sqrt{I_{\varepsilon'}(x, y)/(1 - \varepsilon'^2/4)}$, the old state read under the new $\varepsilon'$ — a ripple across the glide. `Cell::retune(e)` reads that implied amplitude and rescales both coordinates (`s = amp / sqrtf(a2)`) onto the new ellipse of the held peak $x$-amplitude $A$: a homothety that keeps the **amplitude**, not the action — the orbit $I_\varepsilon = A^2(1 - \varepsilon^2/4)$ encloses $\pi A^2\sqrt{1 - \varepsilon^2/4}$, which changes with $\varepsilon$ (Module 1 §1.6.2; the suite's ripple figures are there).
2. **Lagrangian vs. Eulerian Pickup Coordinates on the Toda Ring (`TodaRing::pickup`, `voxo/src/suzu.h`):**
   On `suzu::TodaRing`, each node $i$ has lattice index $\xi = i$ and physical lab position $x(\xi) = \xi + q(\xi)$. Reading the compression in the **Eulerian lab coordinate** $x = \xi + q(\xi)$ (a nonlinear coordinate transformation on the ring whose Jacobian is $1 + \partial_\xi q = 1 + r$) sharpens a hard soliton's density spike where the lattice nearly touches ($r \to -1$) — and that reading was **rejected** at step 83: it put $-43$ dB of content in the top octave, which aliases. What ships reads the compression in the lattice's own coordinate, the pickup standing still against the mean drift ($\xi$ runs back by $\sum p / N$ a step, a Catmull–Rom cubic over the bonds). Two charts, the same physics, two spectra: the coordinate change is audible, and the engine chose the one that does not alias.
3. **Specular Wavefolding in the Stadium Billiard (`suzu::Billiard`, `voxo/src/suzu.h`):**
   In `suzu::Billiard`, the interior flight is the exact harmonic oscillator rotation at $\omega_0$ in $(x_1, y_1)$ and $(x_2, y_2)$. When the orbit strikes the convex stadium boundary $g(x_1, x_2) = 0$ at sub-sample fraction $f \in (0, 1]$, the velocity reflects across the wall normal ($v' = v - 2(v \cdot \hat{n})\hat{n}$, $\det J = +1$ on $\mathbb{R}^4$). For the circular wall ($L = 0$), the system is integrable and acts as an exact 2D **symplectic wavefolder** whose fundamental pitch is set by $\omega_0$ while wall reflections fold the waveform and generate bright overtones (anti-aliased by the two-point polyBLAMP residual of each kink through a one-sample delay — $-80.5$ dB of alias, gate 61).

---

## Lab Instructions (`module_03_lab.html`)

Open [the module's lab](../module_03_lab.html) (a standalone page — a teaching re-implementation, never the engine) in a WebGL2 + WebAudio browser. The lab is split into two interactive parts matching the lecture notes.

### Part 1: Shader Lab — Moser's Homotopy Flow ($\frac{d}{dt}\psi_t^*\omega_t = 0$) & Riemannian vs. Symplectic Duality

The top WebGL2 canvas visualizes a spatially varying closed 2-form $\omega_1 = f(q, p)\,dq \wedge dp$ (with $f(q, p) > 0$ strictly positive everywhere) and integrates **Moser's time-dependent homotopy ODE**:

$$\frac{d}{dt}\psi_t(q, p) = X_t\big(\psi_t(q, p)\big), \qquad X_t(q, p) = \frac{1}{1 + t\big(f(q, p) - 1\big)}\begin{pmatrix} -\alpha_p(q, p) \\ \alpha_q(q, p) \end{pmatrix}, \qquad t \in [0, 1]$$

directly inside the shader (via 16-step RK4 integration from $t = 0$ to the slider value $t \in [0, 1]$):

1. **Morph Warped Metric Cells into Canonical Darboux Squares ($t : 0 \to 1$):**
   - At **$t = 0$** ($\psi_0 = \mathrm{id}$), the grid shows the warped density $f(q, p)$: cells where $f(q, p) > 1$ carry excess symplectic weight ($\omega_1 > \omega_0$), and cells where $0 < f(q, p) < 1$ are rarefied ($\omega_1 < \omega_0$).
   - Drag the **Homotopy Time $t \in [0, 1]$** slider from `0.00` to `1.00` (or click **Animate Homotopy $t : 0 \leftrightarrow 1$**). Watch the shader integrate $X_t$ in real time: dense regions expand and sparse regions contract until, at **$t = 1$**, the pulled-back 2-form $\psi_1^*\omega_1 = f(\psi_1(q,p))\,\det(D\psi_1)\,dq \wedge dp$ becomes **uniformly equal to $1.000$ across the entire canvas**, morphing the warped coordinate grid into uniform canonical Darboux squares!
   - Watch the live GPU readback telemetry in the HUD: as $t \to 1$, **Pullback Defect $\max|(\psi_t^*\omega_1)/\omega_0 - 1|$** drops from its initial warp down to `~0.000`!

2. **Compare 4 Density Profiles $f(q, p)$ & Inspect Kessho's Riemannian Strain $C = D\psi_1^T D\psi_1$:**
   - Switch the **Target 2-Form Density $f(q, p)$** between:
     - **Gaussian Density Lens:** $f(q, p) = 1 + A\,e^{-(r/R)^2}$ (radial Moser flow).
     - **Periodic Chladni Egg-Carton:** $f(q, p) = 1 + A\cos(k q)\cos(k p)$ (2D periodic Moser potential).
     - **Dipole / Asymmetric Pinch:** $f(q, p) = 1 + A\,q\,e^{-(r/R)^2}$ (shifts the Darboux grid asymmetrically while fixing the boundary).
     - **Hamilton's Crystal Drop Habit (`drop_fs`):** Polar density $f(q, p)$ with $m$-blade crystal habit modulation $R(\theta)$.
   - Switch the **View Mode** to **Kessho Riemannian Birefringence ($\sigma - \sigma^{-1}$ at $t = 1$)**! Even at $t = 1$, where the symplectic 2-form is completely flat ($\psi_1^*\omega_1 = \omega_0$, zero symplectic curvature!), you will see vivid **Michel-Lévy interference fringes and Maltese cross brushes** because the Riemannian metric pullback $C = D\psi_1^T D\psi_1$ has non-trivial shear $\sigma > 1$!

### Part 2: Sound Lab — Canonical Timbre Warping via Generating Functions ($S(q, Q)$ & $F_2(q, P)$)

The bottom 48 kHz WebAudio lab lets you apply a nonlinear coordinate transformation $\Phi : (Q, P) \mapsto (q, p)$ to an audio oscillator's phase space and compare **Canonical Generating-Function Warping** ($\oint p\,dq = \oint P\,dQ = 2\pi I$, **0.00 cents detuning**) against **Non-Canonical In-Loop Force Shearing** (`Cell::step_sheared` without table compensation, which detunes up to **+555 cents**!):

1. **Lock the Pitch While Warping the Timbre ($F_2(q, P)$ & $S(q, Q)$):**
   - Click **Start 48 kHz Audio** and sweep the **Warp / Wavefold Depth $g$** slider from `0.00` to `1.00`.
   - Select any of the three **Canonical Generating Functions** ($\det \frac{\partial(q,p)}{\partial(Q,P)} \equiv 1$):
     - **Type-2 Sinusoidal Wavefolder:** $F_2(q, P) = \big(q + \frac{g}{k}\sin(kq)\big)P$ (folds the position waveform $q(t)$ into rich odd/even Buchla-style harmonic overtones while scaling $p(t) = (1 + g\cos(kq))P$ so $\oint p\,dq = 2\pi I$).
     - **Type-1 Cross-Coupled S-Fold:** $S(q, Q)$ twist composition $q = Q + g\sin(2P)$, $p = P - g\sin(2q)$ ( twists the circular $(Q, P)$ orbit into a multi-lobed figure-eight / braid in phase space while preserving enclosed area to machine precision!).
     - **Type-2 Cubic Pinch (`shape_cubic` Cotangent Lift):** $F_2(q, P) = \big(q + \frac{1}{3}g q^3\big)P$.
   - Look at the three synchronized live scopes:
     1. **Phase-Space Loop $(q, p)$ & Enclosed Action $\oint p\,dq$:** Watch the cyan $(q, p)$ curve fold and twist wildly away from the dashed gold circle $(Q, P)$, while the live numerical line integral **Action Ratio $\oint p\,dq / (2\pi I_0)$** stays locked at **`1.0000`**!
     2. **Time-Domain Waveform $q(t)$ (Left) & $p(t)$ (Right):** Watch $q(t)$ crease into a folded wave while its zero-crossings stay pinned at the exact fundamental period $T_0 = 1/f_0$.
     3. **Live FFT Harmonic Spectrum:** Watch upper harmonics ($2f_0, 3f_0, 5f_0, 7f_0 \dots$) bloom by `+35 dB` as $g$ increases, while the fundamental peak $f_0$ never moves by a tenth of a cent!

2. **Compare Against the Non-Canonical In-Loop Shear (`Cell::step_sheared` Red Control):**
   - Switch the mode to **`[RED] Uncalibrated In-Loop Force Shear (Cell::step_sheared)`** and sweep $g$ from `0` to `1`.
   - Because modifying the restoring force inside the loop *without* a canonical coordinate conjugation alters the enclosed action $\oint p\,dq \neq 2\pi I_0$, you will immediately **hear the pitch bend sharp by hundreds of cents** (up to `+110 cents` on cubic and `+555 cents` on triangle fold, exactly as documented in `voxo/src/suzu.h`) and see the enclosed area $\oint p\,dq$ shrink on the phase-space scope!
   - Then toggle **`suzu::shear_period_ratio` 17-Point Calibration Table ON** (`voxo/src/suzu.h`) to hear how `Suzu` pre-calibrates $\varepsilon$ to restore tuning when using `step_sheared`.

---

## Problem Set

### Problem 3.1: Moser's Homotopy ODE for a Radially Varying 2-Form & Hamilton's Crystal Drop

Let $\omega_0 = dq \wedge dp = r\,dr \wedge d\theta$ be the standard area 2-form on $\mathbb{R}^2$, and consider a radially weighted 2-form:

$$\omega_1 = f(r)\,dq \wedge dp = f(r)\,r\,dr \wedge d\theta, \qquad f(r) = 1 + A e^{-r^2 / R^2}, \quad A > -1$$

1. **Explicit 1-Form Primitive $\alpha$ and Moser Vector Field $X_t$:**
   Look for a rotationally symmetric 1-form $\alpha = g(r)\,d\theta$ satisfying $d\alpha = \omega_1 - \omega_0 = \big(f(r) - 1\big)r\,dr \wedge d\theta$ and regular at the origin ($g(0) = 0$).
   - Prove that $g(r) = \int_0^r \big(f(s) - 1\big)s\,ds = \frac{1}{2}A R^2 \big(1 - e^{-r^2/R^2}\big)$, or in Cartesian coordinates:
     $$\alpha = \frac{g(r)}{r^2}\big(-p\,dq + q\,dp\big)$$
   - Show that Moser's equation $i_{X_t}\omega_t = -\alpha$ (where $\omega_t = (1 + t(f(r) - 1))\,r\,dr \wedge d\theta$) yields a purely radial vector field:
     $$X_t(r, \theta) = -\frac{g(r)}{r\,\big(1 + t(f(r) - 1)\big)}\,\frac{\partial}{\partial r}$$
   - Integrate $\frac{dr}{dt} = X_t^r(r)$ explicitly from $t = 0$ to $t = 1$ (hint: compute $\frac{d}{dt}\int_0^{r(t)} (1 + t(f(s)-1))s\,ds$) and show that the time-1 map $r_1 = \psi_1(r_0)$ satisfies the exact finite-area conservation law:
     $$\int_0^{r_1} f(s)\,s\,ds = \frac{1}{2}r_0^2 \qquad \Longleftrightarrow \qquad r_1^2 + \frac{1}{2}A R^2\big(1 - e^{-r_1^2/R^2}\big) = r_0^2$$

2. **Hamilton's 1832 Crystal Drop (`drop_fs`, `site/src/content/docs/operators/drop.mdx`):**
   Verify that in polar Darboux coordinates $(\rho, \theta) = (\frac{1}{2}r^2, \theta)$, the crystal habit drop map
   $$\rho_{src} = \rho - \frac{1}{2}R(\theta)^2, \qquad \theta_{src} = \theta$$
   is generated by the Type-2 generating function $F_2(\theta, \rho_{src}) = \theta\,\rho_{src} + \frac{1}{2}\int_0^\theta R(\vartheta)^2\,d\vartheta$. Compute the Cartesian Jacobian matrix $J = \frac{\partial(x_{src}, y_{src})}{\partial(x, y)}$ for $r \ge R(\theta)$, verify that $\det J = 1$ for any smooth habit $R(\theta) > 0$, and explain why a corner where $R'(\theta)$ jumps (such as the cleavage vertices of the calcite rhomb at `habit_roundness = 0`) creates a discontinuity in $J$ along the vertex ray that Kessho renders as a birefringence cleavage line.

---

### Problem 3.2: Generating Functions, Action Invariance & Zero-Detune Wavefolding

Let $(Q(t), P(t)) = \big(\sqrt{2I}\,\sin(\omega_0 t),\; \sqrt{2I}\,\cos(\omega_0 t)\big)$ be a canonical harmonic oscillator of action $I = E / \omega_0$ and fundamental frequency $\omega_0$.

1. **Type-2 Canonical Point-Cotangent Wavefolder:**
   Consider the Type-2 generating function:
   $$F_2(q, P) = f(q)\,P, \qquad f(q) = q + \frac{g}{k}\sin(kq), \qquad |g| < 1$$
   - Write out the canonical relations $Q = \frac{\partial F_2}{\partial P}$ and $p = \frac{\partial F_2}{\partial q}$, and show that the transformation $(q, p) \mapsto (Q, P)$ is a global diffeomorphism on $\mathbb{R}^2$ whenever $|g| < 1$.
   - Express the pullback Hamiltonian $H(q, p) = H_0(Q(q, p), P(q, p)) = \frac{1}{2}\omega_0\big(Q^2 + P^2\big)$ explicitly in terms of $(q, p)$:
     $$H(q, p) = \frac{1}{2}\omega_0\left[ \left(q + \frac{g}{k}\sin(kq)\right)^2 + \frac{p^2}{\big(1 + g\cos(kq)\big)^2} \right]$$
   - Prove directly from $\oint_{\gamma_E} p\,dq$ (by substituting $Q = f(q)$ and $dq = dQ / f'(q)$ around the closed loop $H(q, p) = E$) that the enclosed phase-space area is $\oint_{\gamma_E} p\,dq = 2\pi E / \omega_0$ **independent of $g$ and $k$**, so every periodic orbit of the highly nonlinear Hamiltonian $H(q, p)$ above has exact fundamental frequency $\omega(E) = \omega_0$!

2. **Type-1 Twist Generating Function for the Chirikov Standard Map (`suzu::Rotor`):**
   Show that the Type-1 generating function
   $$S(\theta, \Theta) = \frac{1}{2\varepsilon}(\Theta - \theta)^2 + \frac{K}{\varepsilon}\cos\theta$$
   generates the Chirikov standard map $p = \frac{\partial S}{\partial \theta}$, $P = -\frac{\partial S}{\partial \Theta}$ (`site/src/content/docs/operators/chirikov.mdx`, `voxo/src/suzu.h`). Under what condition on $(\theta, \Theta)$ is the twist condition $\frac{\partial^2 S}{\partial \theta \partial \Theta} \neq 0$ satisfied?

---

### Problem 3.3: Why `Cell::step_sheared` Detunes & How `Cell::retune` Re-Bases Orbits (`voxo/src/suzu.h`)

1. **First-Order Perturbation of Action for `Cell::step_sheared` (`voxo/src/suzu.h`):**
   In the continuous limit $\varepsilon \to 0$, `Cell::step_sheared` with `shape_cubic` ($s(y) = g y^3$ at unit amplitude) integrates the separable Hamiltonian:
   $$H_g(x, y) = \frac{1}{2}\omega_0\left(x^2 + y^2 + \frac{g}{2}y^4\right)$$
   Notice that $H_g$ modifies the $y$-potential *without* rescaling the conjugate $x$-term by $(1 + g y^2)^{-2}$ as a canonical point transformation would!
   - Using polar coordinates $x = r\cos\varphi, y = r\sin\varphi$, show that to first order in $g$ near the unit orbit $E = \frac{1}{2}\omega_0$, the enclosed action $I(E) = \frac{1}{2\pi}\iint_{H_g(x,y) \le E} dx\,dy$ shrinks by:
     $$I(E) = \frac{E}{\omega_0}\left(1 - \frac{3}{8}g\,\frac{2E}{\omega_0} + \mathcal{O}(g^2)\right)$$
   - Differentiate $I(E)$ to find $\omega(E) = (dI/dE)^{-1}$ at unit amplitude ($2E/\omega_0 = 1$), and verify the comment in `voxo/src/suzu.h` that the cubic shear sharpens the frequency by $\omega / \omega_0 \approx \sqrt{1 + \frac{3}{4}g} \approx 1 + \frac{3}{8}g$ to first order (and why `suzu.h` uses the 17-point numerical `shear_period_ratio` table to eliminate the remaining 110-cent and 555-cent errors at $g = 1$).

2. **Symplectic Orbit Re-Basing in `Cell::retune` (`voxo/src/suzu.h`):**
   Show that the linear transformation $(Q, P) \mapsto (x, y)$ defined by
   $$\begin{pmatrix} x \\ y \end{pmatrix} = \begin{pmatrix} \dfrac{1}{\sqrt{1 - \varepsilon^2/4}} & 0 \\[10pt] \dfrac{\varepsilon / 2}{\sqrt{1 - \varepsilon^2/4}} & 1 \end{pmatrix} \begin{pmatrix} Q \\ P \end{pmatrix}$$
   has determinant $1/\sqrt{1 - \varepsilon^2/4}$ — it is **not** a linear symplectomorphism; it is the conjugation that turns the tilted ellipse into a circle, and the problem is to say exactly that — and pulls the magic circle's shadow invariant $I_\varepsilon(x, y) = x^2 + y^2 - \varepsilon x y$ back to the canonical circle $Q^2 + P^2$ (with peak $x$-amplitude $A = \sqrt{(Q^2 + P^2)/(1 - \varepsilon^2/4)}$). Use this to explain why `Cell::retune(e)` (`s = amp / sqrtf(a2)`) keeps the peak amplitude $A$ constant across a $\pm 48$-semitone MPE glide, whereas `Cell::retune_plain(e)` changes the amplitude by $\sqrt{I_{\varepsilon'}(x, y)/(1 - \varepsilon'^2/4)}\,/A$ — the old state read under the new $\varepsilon'$ — which depends on the phase at the retune and is not a fixed ratio.

