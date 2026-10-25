# **Module 1 Lecture Notes: Exterior Calculus, the Exactness Rule, and Area-Preserving Phase Space**

## **1.1 Historical Prologue: From Crystal Optics to Symplectic Geometry**

Symplectic geometry was born in 1832 not in mechanics, but in crystal optics, when William Rowan Hamilton used his characteristic function to predict **conical refraction** in biaxial crystals—the exact phenomenon implemented in midi-sink’s **Kessho** medium (SUMI\_MEDIUM\_KESSHO, Barite preset). Whether we track light rays through a strained calcite crystal, pigment rings floating on water (Suminagashi), or the phase-space trajectory of an acoustic oscillator (Suzu), the governing structure is a **closed, non-degenerate differential 2-form** $\omega$.

## **1.2 Differential Forms and Symplectomorphisms in 2D and 4D**

Let $M={R}^{2n}$ (or a cylinder/torus ${T}^{*}Q$) be phase space with canonical coordinates \$(q\_1, \\dots, q\_n, p\_1, \\dots, p\_n)\$.

> 1. **The Tautological 1-Form $\theta$:** On a cotangent bundle ${T}^{*}Q$, the canonical 1-form is $\theta =\sum\limits_{i=1}^{n}{p}_{i}d{q}_{i}$. Integrating $\theta$ around a closed loop $\gamma =\partial \Sigma$ measures the mechanical action $I=\frac{1}{2\pi }\oint\limits_{\gamma }^{}pdq$.

> **The Symplectic 2-Form $\omega$:** Taking the exterior derivative yields the canonical 2-form:

> $$\omega =-d\theta =\sum\limits_{i=1}^{n}d{q}_{i}\wedge d{p}_{i}$$  
>   
> By Poincaré’s lemma (${d}^{2}=0$), $\omega$ is **exact** on ${R}^{2n}$ and therefore **closed**:

> $$d\omega =0$$

> 2. 

> **Symplectomorphisms and $\det J=1$:** A smooth map $\Phi :M\rightarrow M$ is a **symplectomorphism** if it preserves the 2-form under pullback: ${\Phi }^{*}\omega =\omega$. In 2D ($n=1$, the libsumi canvas $(x,y)$ or a single Suzu oscillator cell $(x,y)$), $\omega =dx\wedge dy$ is the oriented area form. For any map $\Phi (x,y)=(u(x,y),v(x,y))$ with Jacobian matrix  
> \$\$J \= D\\Phi \= \\begin{pmatrix} \\frac{\\partial u}{\\partial x} & \\frac{\\partial u}{\\partial y} \\\\ \\frac{\\partial v}{\\partial x} & \\frac{\\partial v}{\\partial y} \\end{pmatrix},\$\$  
> the pullback is Directly evaluated via the exterior wedge product:

> $${\Phi }^{*}(dx\wedge dy)=du\wedge dv=\left({\frac{\partial u}{\partial x}\frac{\partial v}{\partial y}-\frac{\partial u}{\partial y}\frac{\partial v}{\partial x}}\right)dx\wedge dy=(\det J)dx\wedge dy$$

> 3.   
>    Thus, on a 2D manifold, $\Phi$ **is symplectic if and only if $\det J=1$ everywhere**.

## **1.3 Why Eulerian Grids Fail and Why libsumi Stores Displacements**

If you advect pigment on a grid using an Eulerian Navier–Stokes velocity solver, bilinear interpolation of pigment density introduces numerical diffusion that blurs sharp Suminagashi rings into mud within tens of frames.  
Instead, libsumi models deformation as a **group composition of closed-form inverse-lookup symplectomorphisms** on ping-pong textures (tex\_A, tex\_B):  
Rather than storing raw pre-image coordinates $(u,v)$—where RGBA16F half-float precision degrades to ${2}^{-11}$ across the outer half of the canvas—each texel stores the **accumulated displacement** from identity (libsumi 1.2.0+):

$$d(P)={P}_{pre}(P)-P=(u-x,v-y)$$

*   
* At rest, $d(P)=(0,0)$, giving ${2}^{-24}$ canvas-height precision.

When a new deformation pass with inverse map ${\Phi }^{-1}(P)={P}_{src}$ is applied, the shader re-bases the lookup:

$$d'(P)=d({P}_{src})+({P}_{src}-P)$$

*   
* Because the Jacobian of a composition is the product of Jacobians, $J({\Phi }_{2}\circ {\Phi }_{1})=(J{\Phi }_{2})(J{\Phi }_{1})$, composing any number of $\det J=1$ passes preserves $\det J=1$ to floating-point rounding.

## **1.4 The Symplectic Exactness & Localization Rule (KESSHO §10.2)**

In interactive graphics and audio, we want deformations that are **spatially localized** (decaying away from a note’s cell center). However, multiply a standard symplectic kick or shear by a naive radial window $W(r)$, and you immediately violate $d\omega =0$.

### **Proposition 1.1 (The Windowed Kick Defect)**

Consider action-angle polar coordinates $(\rho ,\theta )$ about a drop center, where $\rho =\frac{1}{2}{r}^{2}$ so that $dx\wedge dy=rdr\wedge d\theta =d\rho \wedge d\theta$ is canonical. Suppose we apply a radially windowed action kick without modifying the angle:

$${\rho }_{1}=\rho -W(\rho )\frac{\partial V(\theta )}{\partial \theta },{\theta }_{1}=\theta$$

Then the Jacobian matrix is:

\$\$J \= \\begin{pmatrix} \\frac{\\partial \\rho\_1}{\\partial \\rho} & \\frac{\\partial \\rho\_1}{\\partial \\theta} \\\\ \\frac{\\partial \\theta\_1}{\\partial \\rho} & \\frac{\\partial \\theta\_1}{\\partial \\theta} \\end{pmatrix} \= \\begin{pmatrix} 1 \- W'(\\rho)V'(\\theta) & \-W(\\rho)V''(\\theta) \\\\ 0 & 1 \\end{pmatrix} \\implies \\det J \= 1 \- W'(\\rho)V'(\\theta) \\neq 1\$\$  
Any spatial window on the coordinate being kicked breaks area conservation proportionally to $(windowslope)\times (kick)$.  
This yields the fundamental design theorem of libsumi (§10.2):  
**The Exactness Rule:** An operator is both **exact ($\det J=1$ at any amplitude)** and **localized** if and only if it belongs to one of four canonical constructions:

### **Family 1: Pure Coordinate Shears (Tines, Cleavage Profiles, Sine Ripple)**

In a local frame $(x,y)$ aligned with a stroke direction $\hat{D}$, let:

$${x}_{src}=x-F(y),{y}_{src}=y$$

Because $F$ depends **only on $y$** and ignores $x$, the Jacobian is upper-triangular with ones on the diagonal:

\$\$J \= \\begin{pmatrix} 1 & \-F'(y) \\\\ 0 & 1 \\end{pmatrix} \\implies \\det J \= 1 \\quad \\forall\\, F(y)\$\$  
This holds whether $F(y)$ is Jaffer’s smooth tine \$\\frac{z\\alpha}{\\alpha \+ \\vert{}y\\vert{}}\$, the sine ripple $A\sin (ky+\phi )$, Baumhauer’s piecewise-linear calcite twin-kink (SUMI\_TINE\_TWIN\_KINK), or muscovite’s terraced phyllosilicate slip (SUMI\_TINE\_TERRACE).

### **Family 2: Twist Maps in Action-Angle Coordinates (Vortices, Lamb–Oseen, Torsion, Chladni)**

In canonical polar coordinates $(\rho ={r}^{2}/2,\theta )$, rotate the angle by a function of $\rho$ alone:

$${\rho }_{src}=\rho ,{\theta }_{src}=\theta -\Theta (\rho )$$

Then:

\$\$J\_{(\\rho,\\theta)} \= \\begin{pmatrix} 1 & 0 \\\\ \-\\Theta'(\\rho) & 1 \\end{pmatrix} \\implies \\det J \= 1\$\$  
Twist maps are the **only** explicit family that is simultaneously exact and compactly supported in $r$. This is why the Lamb–Oseen vortex $\Theta (r)=\frac{\Gamma \Delta t}{2\pi {r}^{2}}(1-{e}^{-{r}^{2}/{r}_{c}^{2}})$, Wave Torsion $\Theta (r)=A\sin (kr-\phi ){e}^{-r/R}$, and the Chladni eddy ring $\Theta (\rho )={\theta }_{0}(4{\rho }^{2}(1-{\rho }^{2}){)}^{2}$ never fold and require zero sub-stepping at any amplitude.

### **Family 3: Angle-Generated Kicks — The Symplectic Crystal-Drop Theorem (§10.7.8)**

Suppose we exchange the roles of $\rho$ and $\theta$ in $(\rho ={r}^{2}/2,\theta )$ and kick $\rho$ by a potential $V(\theta )=\frac{1}{2}R(\theta {)}^{2}$ that depends **only on the angle $\theta$**:

$${\rho }_{src}=\rho -\frac{1}{2}R(\theta {)}^{2},{\theta }_{src}=\theta (forr\geq R(\theta ))$$

In Cartesian radius $r=\sqrt{2\rho }$, this maps each ray $\theta$ monotonically onto itself via ${r}_{src}=\sqrt{{r}^{2}-R(\theta {)}^{2}}$. Its Jacobian in $(\rho ,\theta )$ is:

\$\$J\_{(\\rho,\\theta)} \= \\begin{pmatrix} 1 & \-R(\\theta)R'(\\theta) \\\\ 0 & 1 \\end{pmatrix} \\implies \\det J \= 1 \\text{ identically\!}\$\$

* **Consequence:** A drop does not need to be circular to be an exact symplectomorphism. Any star-shaped crystal habit $R(\theta )>0$—such as a Calcite cleavage rhomb (SUMI\_HABIT\_RHOMB), a Muscovite hexagon (SUMI\_HABIT\_HEX), or a $m$\-bladed Barite rosette $R(\theta )={r}_{0}(1+\beta {\cos }^{2p}(m\theta -{\theta }_{0}))$ (SUMI\_HABIT\_ROSETTE)—conserves area identically. Where the crystal has sharp vertices ($R'(\theta )$ jumps), $J$ jumps across the vertex ray, creating a physical birefringence cleavage crease in Kessho\!

### **Family 4: Invariant-Streamline Windowing & Generating Functions**

**Hamiltonian Pinch (§4.3(5)):** A global hyperbolic saddle ${x}_{src}=x{e}^{k},{y}_{src}=y{e}^{-k}$ is generated by the Hamiltonian $H(x,y)=xy$. Because $s=xy$ is conserved along every trajectory (${x}_{src}{y}_{src}=xy=s$), windowing the exponent by \$w(s) \= e^{-\\vert{}s\\vert{}/S}\$:

$${x}_{src}=x{e}^{+kw(xy)},{y}_{src}=y{e}^{-kw(xy)}$$

yields:

$$\det J={e}^{kw}(1+kxyw')\cdot {e}^{-kw}(1-kxyw')-(k{x}^{2}w'{e}^{kw})(-k{y}^{2}w'{e}^{-kw})=(1-{k}^{2}{s}^{2}(w'{)}^{2})+{k}^{2}{s}^{2}(w'{)}^{2}=1$$

* 

**Type-2 Generating Functions (SUMI\_DEFORM\_ARNOLD, §10.8.1):** When a kick *must* be radially windowed by $W({\rho }_{1})$, we derive the map from a mixed-variable generating function ${F}_{2}(\theta ,{\rho }_{1})=\theta {\rho }_{1}+\frac{K}{q}W({\rho }_{1})\cos (q\theta )$:

$$\rho =\frac{\partial {F}_{2}}{\partial \theta }={\rho }_{1}-KW({\rho }_{1})\sin (q\theta ),{\theta }_{1}=\frac{\partial {F}_{2}}{\partial {\rho }_{1}}=\theta +\frac{K}{q}W'({\rho }_{1})\cos (q\theta )$$

*   
  Here, the radial slope $W'({\rho }_{1})$ is compensated by an angular twist, keeping $\det J=1$ exactly when solved via 5-step scalar Newton iteration in the fragment shader.

## **1.5 Reading the Jacobian $J$ with Shaders: Sumi, Anod, and Kessho (§10.7)**

From the stored displacement field $d(x,y)=(u-x,v-y)$, the composite shader reconstructs the inverse-lookup Jacobian $J=I+\nabla d$ via finite differences:

\$\$J \= \\begin{pmatrix} 1 \+ \\frac{\\partial d\_x}{\\partial x} & \\frac{\\partial d\_x}{\\partial y} \\\\ \\frac{\\partial d\_y}{\\partial x} & 1 \+ \\frac{\\partial d\_y}{\\partial y} \\end{pmatrix}\$\$  
Taking the polar decomposition $J={R}_{\phi }U(\psi ,\lambda )$, where ${R}_{\phi }$ is a rotation by $\phi$ and $U$ is symmetric positive-definite with principal stretches $\lambda \geq 1\geq {\lambda }^{-1}$ along principal strain axis $\psi$, the left Cauchy–Green strain tensor of the forward flow is $C={J}^{T}J$.  
Because $\det J=1$, all spectral invariants of $J$ and $C$ reduce to elementary combinations of the Frobenius norm $\parallel J\parallel {}_{F}^{2}={J}_{11}^{2}+{J}_{12}^{2}+{J}_{21}^{2}+{J}_{22}^{2}$ and the trace \$\\operatorname{tr} J \= J\_{11} \+ J\_{22}\$:  
> **Principal Strain / Birefringence Retardation ($\sigma$, Anod & Kessho Layer A):** Since \$\\lambda^2 \+ \\lambda^{-2} \= \\operatorname{tr}(J^T J) \= \\Vert{}J\\Vert{}\_F^2\$, we have $(\lambda -{\lambda }^{-1}{)}^{2}={\lambda }^{2}+{\lambda }^{-2}-2=\parallel J\parallel {}_{F}^{2}-2$. Thus the scalar shear strain is:

> $$\sigma =\lambda -{\lambda }^{-1}=\sqrt{\parallel J\parallel {}_{F}^{2}-2}$$

> 1.   
>    and the principal strain orientation $\psi$ is:  
>    \$\$\\psi \= \\frac{1}{2}\\operatorname{atan2}\\\!\\Big(2(J\_{11}J\_{12} \+ J\_{21}J\_{22}),\\; (J\_{11}^2 \+ J\_{21}^2) \- (J\_{12}^2 \+ J\_{22}^2)\\Big)\$\$  
>    In **Kessho Layer A**, a crystal under crossed polarizers at angle $\alpha$ transmits intensity:  
>    \$\$I\_A \= \\sin^2\\\!\\big(2(\\psi \- \\alpha)\\big) \\cdot I\_{\\text{backlight}} \\cdot \\text{ML}(\\Gamma \\sigma \+ \\Gamma\_0)\$\$  
>    where ${\sin }^{2}(2(\psi -\alpha ))$ paints the dark **isoclinic Maltese cross** in vortex shear annuli, and $ML(\Gamma \sigma )$ samples the CIE 1931 Michel–Lévy interference chart.  
> 2. **Trace Discriminant & Elliptic/Hyperbolic Partition (Kessho Layer B):** The eigenvalues of $J\in SL(2,R)$ satisfy \$\\mu^2 \- (\\operatorname{tr} J)\\mu \+ 1 \= 0\$, with discriminant:  
>    \$\$\\Delta \= (\\operatorname{tr} J)^2 \- 4\$\$  
   * **Elliptic (\$\\Delta \< 0 \\iff \\vert{}\\operatorname{tr} J\\vert{} \< 2\$):** Purely rotational/oscillatory motion ($\mu ={e}^{\pm i\phi }$, vortex cores, Chladni eddies)—rendered as a calm, cool tint. Note that rigid rotation has \$C \= I \\implies \\sigma \= 0\$ (dark in Anod and Kessho Layer A), so \$\\operatorname{tr} J \= 2\\cos\\phi\$ is the *only* local observable that detects rigid elliptic rotation\!  
   * **Hyperbolic (\$\\Delta \> 0 \\iff \\vert{}\\operatorname{tr} J\\vert{} \> 2\$):** Exponential stretching/pinching along unstable manifolds (saddles, wakes, Chirikov chaos)—rendered as a warm tint.  
   * **Parabolic (\$\\Delta \= 0 \\iff \\vert{}\\operatorname{tr} J\\vert{} \= 2\$):** The order-to-chaos separatrix boundary.  
> 3. **The Collapsed Lagrangian Caustic & The $z=1$ Flatness Gate (§10.7.3):** Refracting rays through the displacement gradient $\nabla d=J-I$ onto a bed at optical depth $z$ focuses light inverse-proportionally to the ray-map determinant \$\\det(I \+ z\\nabla\\mathbf{d})\$. For any $2\times 2$ matrix $J$, expanding $\det ((1-z)I+zJ)$ gives:  
>    \$\$\\det(I \+ z\\nabla\\mathbf{d}) \= (1 \- z)^2 \+ z(1 \- z)\\operatorname{tr} J \+ z^2 \\det J\$\$  
   * **When $\det J=1$ (Symplectic):** $(1-z{)}^{2}+{z}^{2}=1-2z(1-z)$, so the determinant collapses to Kessho’s one-line formula:  
     \$\$\\det(I \+ z\\nabla\\mathbf{d}) \= 1 \+ z(1 \- z)(\\operatorname{tr} J \- 2)\$\$  
     The caustic fold ($\det =0$) sits on the iso-trace contour \$\\operatorname{tr} J \= 2 \+ \\frac{1}{z(z-1)}\$ for $z>1$, scanning through the hyperbolic regime toward the parabolic horizon \$\\operatorname{tr} J \= 2^+\$ as $z\rightarrow \infty$.  
   * **When $z=1$ (--kessho-flat Diagnostic):** The uncollapsed determinant reduces to \$\\det(I \+ 1\\cdot\\nabla\\mathbf{d}) \= \\det J\$\! Therefore, evaluating the full determinant at $z=1$ is a direct visual detector for $d\omega \ne 0$: any exact operator renders flat, whereas a naively windowed kick immediately lights up as a bright defect.

## **1.6 Phase Space in Audio: Suzu’s Magic Circle, Retuning, and Twisted 2-Forms (§13)**

Now drop from the $512\times 512$ canvas to a single oscillator voice in **Suzu** (voxo/src/suzu.h).

### **1\. The Symplectic Magic Circle vs. Naive Euler (§13.2)**

A harmonic oscillator of frequency $f$ at sample rate ${f}_{s}$ has step parameter $\varepsilon =2\sin (\pi f/{f}_{s})$.

* **Naive Simultaneous Euler (update\_mode \= 1, Red Control):**  
  \$\$x\_{n+1} \= x\_n \- \\varepsilon y\_n, \\qquad y\_{n+1} \= y\_n \+ \\varepsilon x\_n \\implies J \= \\begin{pmatrix} 1 & \-\\varepsilon \\\\ \\varepsilon & 1 \\end{pmatrix}, \\quad \\det J \= 1 \+ \\varepsilon^2 \> 1\$\$  
  Because $\det J>1$, phase-space area expands by $1+{\varepsilon }^{2}$ every sample. At $f=440Hz$, amplitude blows up past $+0.1dB$ in under a second and reaches $+690dB$ **in 10 minutes**.

**Symplectic Leapfrog / Magic Circle (update\_mode \= 0):** Using the already-updated ${x}_{n+1}$ in the $y$ update composes two exact shears:

$${x}_{n+1}={x}_{n}-\varepsilon {y}_{n},{y}_{n+1}={y}_{n}+\varepsilon {x}_{n+1}=\varepsilon {x}_{n}+(1-{\varepsilon }^{2}){y}_{n}$$

*   
  \$\$J \= \\begin{pmatrix} 1 & 0 \\\\ \\varepsilon & 1 \\end{pmatrix} \\begin{pmatrix} 1 & \-\\varepsilon \\\\ 0 & 1 \\end{pmatrix} \= \\begin{pmatrix} 1 & \-\\varepsilon \\\\ \\varepsilon & 1 \- \\varepsilon^2 \\end{pmatrix} \\implies \\det J \= 1 \\text{ identically\!}\$\$  
  Moreover, \$\\operatorname{tr} J \= 2 \- \\varepsilon^2 \= 2 \- 4\\sin^2(\\pi f / f\_s) \= 2\\cos(2\\pi f / f\_s) \\in (-2, 2)\$ for all $f<{f}_{s}/2$. Thus \$\\Delta \= (\\operatorname{tr} J)^2 \- 4 \< 0\$: the oscillator is strictly **elliptic**, with exact discrete rotation angle $\phi =2\pi f/{f}_{s}$ (zero tuning error: $0.000cent$ across MIDI 21–108)\!

### **2\. The Skewed Symplectic Invariant & MPE Retuning (Cell::retune)**

While $\det J=1$ guarantees bounded orbits, the Magic Circle’s invariant level curves are not circles ${x}^{2}+{y}^{2}={A}^{2}$, but $\varepsilon$**\-sheared ellipses**:

$${I}_{\varepsilon }(x,y)={x}^{2}+{y}^{2}-\varepsilon xy={A}^{2}\left({1-\frac{{\varepsilon }^{2}}{4}}\right)$$

When an MPE performer glides a note by $\pm 48$ semitones, $\varepsilon$ changes continuously. If you change $\varepsilon \rightarrow \varepsilon '$ without adjusting $(x,y)$, the state jumps onto a new ellipse with a different amplitude (causing $0.630dB$ of gain ripple on a jump). Suzu’s Cell::retune applies an instantaneous symplectic shear-scale transformation to re-base $(x,y)$ onto the orbit of identical amplitude $A$ under $\varepsilon '$, reducing glide ripple to $0.082dB$:

$$Givenu=x,{v}_{orth}=\frac{y-\frac{\varepsilon }{2}x}{\sqrt{1-{\varepsilon }^{2}/4}},{y}_{new}=\frac{\varepsilon '}{2}x+{v}_{orth}\sqrt{1-(\varepsilon '{)}^{2}/4}$$

### **3\. Non-Canonical Closed 2-Forms: The Boris–Leapfrog Magnetic Gyroscope (voice\_kind 11, §13.13)**

Must a symplectic 2-form always be the canonical ${\omega }_{0}=d{p}_{1}\wedge d{x}_{1}+d{p}_{2}\wedge d{x}_{2}$? In Module 1’s graduate extension (and TO\_PROJECT\_SPEC.md §13.13), we introduce a charged 2D oscillator in a perpendicular magnetic field $B({x}_{1},{x}_{2})$. The magnetic Lorentz force is encoded not by changing the Hamiltonian $H=\frac{1}{2}|p{|}^{2}+V(x)$, but by **twisting the symplectic 2-form**:

$${\omega }_{B}=d{p}_{1}\wedge d{x}_{1}+d{p}_{2}\wedge d{x}_{2}+B({x}_{1},{x}_{2})d{x}_{1}\wedge d{x}_{2}$$

On ${R}^{4}$, $d(Bd{x}_{1}\wedge d{x}_{2})=\frac{\partial B}{\partial {x}_{1}}d{x}_{1}\wedge d{x}_{1}\wedge d{x}_{2}+\frac{\partial B}{\partial {x}_{2}}d{x}_{2}\wedge d{x}_{1}\wedge d{x}_{2}=0$, so ${\omega }_{B}$ **is closed ($d{\omega }_{B}=0$) for any smooth magnetic field $B({x}_{1},{x}_{2})$**\! Stepping this system via **Boris Leapfrog** (half potential kick $\rightarrow$ exact Cayley rotation by ${\theta }_{B}=2\arctan (B\Delta t/2)$ $\rightarrow$ half kick) preserves phase-space volume ($\det J=1$) identically and splits degenerate 2D oscillator modes into a chiral **Zeeman doublet**:

$${\omega }_{\pm }=\sqrt{{\omega }_{0}^{2}+({\omega }_{B}/2{)}^{2}}\pm \frac{{\omega }_{B}}{2}$$

Sending $({x}_{1},{x}_{2})$ directly to stereo $(L,R)$ and to trace/ produces a phase-locked stereo rosette that precesses across the canvas as polyphonic pressure raises $B$.

# **Module 1 Lab: The Exactness, Kessho Flatness, and Suzu Phase-Space Lab**

### **Lab Objectives & Deliverable Tasks**

> 1. **Task 1 (Shader — The Symplectic Crystal Drop & Cleavage Creases):** Inside deform.glsl (or the standalone WebGL2 lab harness below), verify that replacing ${r}_{0}$ with a **Calcite Rhomb** ($78.{1}^{\circ }/101.{9}^{\circ }$) or **Barite 5-Bladed Rosette** profile $R(\theta )$ in ${\rho }_{src}=\rho -\frac{1}{2}R(\theta {)}^{2}$ passes the $z=1$ flatness gate (--kessho-flat) with $\det J=1$ everywhere outside the drop rim, and observe how unrounded corners ($R'(\theta )$ jumps) generate sharp birefringence cleavage rays in Kessho Layer A.  
> 2. **Task 2 (Shader — Catching the Non-Symplectic Leak at $z=1$):** Compare the **Hamiltonian Pinch** ($w(s)={e}^{-|xy|/S}$) against a **Naively Radial-Windowed Kick** (${\rho }_{src}=\rho -k{e}^{-{r}^{2}/{R}^{2}}\sin (3\theta )$, ${\theta }_{src}=\theta$). Toggle the **Kessho $z=1$ Flatness Gate** (u\_viewMode \= 2): prove analytically and visually why the Hamiltonian Pinch remains dark/flat ($\det J=1$) while the radial kick glows bright red/magenta ($\det J=1-W'(\rho )V'(\theta )\ne 1$).  
> 3. **Task 3 (Audio — Suzu Magic Circle, Cell::retune, and Boris Gyroscope):** Audition and inspect the phase-plane scope of:  
   * update\_mode \= 1 (Naive Euler, $\det J=1+{\varepsilon }^{2}$) vs. update\_mode \= 0 (Symplectic Magic Circle, $\det J=1$).  
   * retune\_mode \= 1 (Plain $\varepsilon$ update during an MPE pitch bend) vs. retune\_mode \= 0 (Cell::retune symplectic ellipse re-basing).  
   * Raising the magnetic 2-form twist $B$ in the **4D Boris–Leapfrog Gyroscope** (voice\_kind 11\) to hear the Zeeman frequency splitting ${\omega }_{\pm }$ and watch the stereo spirograph precess on the sumi\_set\_scope overlay.

### **Complete Single-File Lab 1 Starter (module1\_kessho\_suzu\_lab.html)**

Save the code below as module1\_kessho\_suzu\_lab.html and open it in any browser. It implements libsumi’s exact ping-pong displacement field (u \- x, v \- y, ink, aux) in RGBA32F, **Kessho Layer A (Michel–Lévy birefringence \+ isoclinic Maltese cross)**, **Layer B (trace discriminant \$\\Delta \= (\\operatorname{tr} J)^2 \- 4\$ \+ collapsed Lagrangian caustic)**, the **\--kessho-flat ($z=1$) $\det J$ diagnostic**, and a WebAudio **Suzu Magic Circle \+ Boris Gyroscope** synthesizer with live sumi\_set\_scope phase-space trace.

HTML  
\<\!DOCTYPE **html**\>  
\<html lang\="en"\>  
\<head\>  
\<meta charset\="UTF-8"\>  
\<title\>Module 1 Lab: Symplectic 2-Forms, Kessho Optics & Suzu Phase Space\</title\>  
\<style\>  
  :root {  
    \--bg: \#0b0d12; \--panel: \#141821; \--border: \#262c3a;  
    \--text: \#e2e8f0; \--muted: \#94a3b8; \--accent: \#38bdf8; \--warn: \#f43f5e;  
  }  
  \* { box-sizing: border-box; margin: 0; padding: 0; }  
  body {  
    background: var(--bg); color: var(--text);  
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;  
    display: flex; height: 100vh; overflow: hidden;  
  }  
  \#sidebar {  
    width: 390px; background: var(--panel); border-right: 1px solid var(--border);  
    padding: 16px; overflow-y: auto; display: flex; flex-direction: column; gap: 14px;  
  }  
  h1 { font-size: 14px; color: var(--accent); letter-spacing: 0.04em; text-transform: uppercase; }  
  h2 { font-size: 12px; color: var(--muted); text-transform: uppercase; margin-bottom: 6px; border-bottom: 1px solid var(--border); padding-bottom: 4px; }  
  .control-group { display: flex; flex-direction: column; gap: 8px; font-size: 12px; }  
  .row { display: flex; justify-content: space-between; align-items: center; gap: 8px; }  
  select, button, input\[type="range"\] {  
    background: \#1e2433; color: var(--text); border: 1px solid var(--border);  
    border-radius: 4px; padding: 5px 8px; font-family: inherit; font-size: 11px;  
  }  
  input\[type="range"\] { flex: 1; accent-color: var(--accent); padding: 0; }  
  button { cursor: pointer; font-weight: 600; transition: background 0.15s; }  
  button:hover { background: \#2d374d; }  
  button.danger { border-color: var(--warn); color: \#fda4af; }  
  .readout {  
    background: \#0d1017; border: 1px solid var(--border); border-radius: 4px;  
    padding: 8px; font-size: 11px; line-height: 1.5; color: \#cbd5e1;  
  }  
  \#canvas-wrap { flex: 1; position: relative; display: flex; align-items: center; justify-content: center; }  
  canvas { width: 100%; height: 100%; display: block; cursor: crosshair; }  
  \#hud {  
    position: absolute; top: 12px; left: 12px; background: rgba(11,13,18,0.82);  
    border: 1px solid var(--border); padding: 8px 12px; border-radius: 6px;  
    font-size: 11px; pointer-events: none; line-height: 1.5;  
  }  
\</style\>  
\</head\>  
\<body\>

\<div id\="sidebar"\>  
  \<h1\>Module 1 · Kessho & Suzu Lab\</h1\>

  \<div class\="control-group"\>  
    \<h2\>1\. Medium & Jacobian Reader (composite.glsl)\</h2\>  
    \<div class\="row"\>  
      \<label\>Medium View\</label\>  
      \<select id\="viewMode"\>  
        \<option value\="0"\>Kessho (Layer A Birefringence \+ Layer B Caustic)\</option\>  
        \<option value\="1"\>Anod (Strain σ \= λ \- 1/λ \+ Deformed Grid)\</option\>  
        \<option value\="2"\>\--kessho-flat (z \= 1 Exactness Gate: \|det J \- 1\|)\</option\>  
        \<option value\="3"\>Sumi (Pre-image Washi Marbling Rings)\</option\>  
      \</select\>  
    \</div\>  
    \<div class\="row"\>  
      \<label\>Polarizer α (rad)\</label\>  
      \<input type\="range" id\="polarizer" min\="0" max\="1.5708" step\="0.01" value\="0.0"\>  
      \<span id\="polarizerVal"\>0.00\</span\>  
    \</div\>  
    \<div class\="row"\>  
      \<label\>Caustic Depth z\</label\>  
      \<input type\="range" id\="causticZ" min\="1.05" max\="8.0" step\="0.05" value\="2.80"\>  
      \<span id\="causticZVal"\>2.80\</span\>  
    \</div\>  
    \<div class\="row"\>  
      \<label\>Retardation Γ (nm)\</label\>  
      \<input type\="range" id\="retard" min\="200" max\="4000" step\="50" value\="2000"\>  
      \<span id\="retardVal"\>2000\</span\>  
    \</div\>  
  \</div\>

  \<div class\="control-group"\>  
    \<h2\>2\. Click Deformation Operator (deform.glsl)\</h2\>  
    \<div class\="row"\>  
      \<label\>Operator\</label\>  
      \<select id\="opMode"\>  
        \<option value\="0"\>Crystal Drop: Barite Rosette (Exact: V(θ)=½R(θ)²)\</option\>  
        \<option value\="1"\>Crystal Drop: Calcite Rhomb (Exact \+ Corner Crease)\</option\>  
        \<option value\="2"\>Lamb–Oseen Swirl (Exact Twist: θ(r))\</option\>  
        \<option value\="3"\>Hamiltonian Pinch (Exact: Streamline w(xy))\</option\>  
        \<option value\="4"\>\[RED CONTROL\] Windowed Kick (Broken: dω ≠ 0\)\</option\>  
      \</select\>  
    \</div\>  
    \<div class\="row"\>  
      \<label\>Habit Roundness\</label\>  
      \<input type\="range" id\="roundness" min\="0.0" max\="1.0" step\="0.02" value\="0.15"\>  
      \<span id\="roundnessVal"\>0.15\</span\>  
    \</div\>  
    \<div class\="row"\>  
      \<button id\="dipBtn"\>Paper Dip (Reset Field d \= 0\)\</button\>  
    \</div\>  
  \</div\>

  \<div class\="control-group"\>  
    \<h2\>3\. Suzu Phase-Space Synth & Boris 2-Form\</h2\>  
    \<div class\="row"\>  
      \<button id\="audioBtn"\>Start Suzu Audio \+ Scope Overlay\</button\>  
    \</div\>  
    \<div class\="row"\>  
      \<label\>Integrator\</label\>  
      \<select id\="suzuMode"\>  
        \<option value\="0"\>Symplectic Magic Circle (det J \= 1\)\</option\>  
        \<option value\="1"\>\[RED\] Naive Simultaneous Euler (det J \= 1 \+ ε²)\</option\>  
      \</select\>  
    \</div\>  
    \<div class\="row"\>  
      \<label\>MPE Retune Rule\</label\>  
      \<select id\="retuneMode"\>  
        \<option value\="0"\>Cell::retune (Re-base I\_ε \= x²+y²-εxy)\</option\>  
        \<option value\="1"\>Plain Retune (Uncompensated Ellipse Jump)\</option\>  
      \</select\>  
    \</div\>  
    \<div class\="row"\>  
      \<label\>Pitch Glide (Hz)\</label\>  
      \<input type\="range" id\="freq" min\="110" max\="880" step\="1" value\="220"\>  
      \<span id\="freqVal"\>220\</span\>  
    \</div\>  
    \<div class\="row"\>  
      \<label\>Magnetic 2-Form B\</label\>  
      \<input type\="range" id\="magB" min\="0.0" max\="180.0" step\="1.0" value\="0.0"\>  
      \<span id\="magBVal"\>0.0\</span\>  
    \</div\>  
    \<div class\="row"\>  
      \<button id\="pluckBtn"\>Re-Strike Orbit (A \= 0.5)\</button\>  
    \</div\>  
  \</div\>

  \<div class\="readout" id\="telemetry"\>  
    Click anywhere on the canvas to apply the selected operator.\<br\>  
    Switch to \<b\>\--kessho-flat (z=1)\</b\> to test whether det J \= 1 holds\!  
  \</div\>  
\</div\>

\<div id\="canvas-wrap"\>  
  \<canvas id\="glcanvas" width\="768" height\="768"\>\</canvas\>  
  \<canvas id\="scopecanvas" width\="768" height\="768" style\="position:absolute;top:0;left:0;pointer-events:none;"\>\</canvas\>  
  \<div id\="hud"\>  
    \<div\>\<b\>Stored Field:\</b\> d(x,y) \= (u \- x, v \- y, ink, aux) \[RGBA32F\]\</div\>  
    \<div id\="hudStatus"\>det J \= 1.0000 \| Δ \= (tr J)² \- 4 \| Suzu: Idle\</div\>  
  \</div\>  
\</div\>

\<script\>  
// \============================================================================  
// PART 1: WEBGL2 PING-PONG DISPLACEMENT ENGINE (libsumi \+ Kessho conventions)  
// \============================================================================  
const canvas \= document.getElementById('glcanvas');  
const gl \= canvas.getContext('webgl2');  
if (\!gl \|\| \!gl.getExtension('EXT\_color\_buffer\_float') \|\| \!gl.getExtension('OES\_texture\_float\_linear')) {  
  alert('WebGL2 with float textures is required.');  
}

const SIM\_RES \= 512;  
let dropCounter \= 0;

const vsSource \= \`\#version 300 es  
in vec2 a\_pos;  
out vec2 v\_st;  
void main() {  
  // libsumi §4.6 orientation convention: texture space, v grows downward  
  v\_st \= vec2(a\_pos.x \* 0.5 \+ 0.5, 0.5 \- a\_pos.y \* 0.5);  
  gl\_Position \= vec4(a\_pos, 0.0, 1.0);  
}\`;

// deform.glsl: Exact Crystal Drop, Swirl, Hamiltonian Pinch, and Red-Control Broken Kick  
const deformFsSource \= \`\#version 300 es  
precision highp float;  
in vec2 v\_st;  
out vec4 outField;

uniform sampler2D u\_tex;  
uniform int u\_op;  
uniform vec2 u\_center;  
uniform float u\_radius;  
uniform float u\_roundness;  
uniform float u\_parity;  
uniform float u\_aux;

// Calcite cleavage rhomb support function R(theta) (Kessho §10.7.8)  
float habitRhomb(float th, float r0, float roundness) {  
  float a1 \= 0.0, a2 \= 1.3631; // 78.1 deg cleavage angle  
  vec4 c \= vec4(cos(th \- a1), cos(th \- a2), cos(th \- a1 \- 3.14159265), cos(th \- a2 \- 3.14159265));  
  float k \= mix(28.0, 4.0, roundness);  
  float lse \= 0.0;  
  for (int i \= 0; i \< 4; i++) {  
    float ci \= max(c\[i\], 0.05);  
    lse \+= exp(-k \* (r0 / ci));  
  }  
  return \-log(lse / 4.0) / k;  
}

// Barite 5-blade rosette R(theta) \= r0 \* (1 \+ beta \* cos^4(5\*theta))  
float habitRosette(float th, float r0, float roundness) {  
  float c \= cos(5.0 \* th);  
  float blade \= pow(c \* c, mix(3.0, 1.0, roundness));  
  return r0 \* (0.72 \+ 0.45 \* blade);  
}

void main() {  
  vec2 P \= v\_st;  
  vec2 rel \= P \- u\_center;  
  float r \= length(rel);  
  float th \= atan(rel.y, rel.x);  
  vec2 P\_src \= P;

  if (u\_op \== 0 \|\| u\_op \== 1\) {  
    // THE SYMPLECTIC CRYSTAL-DROP THEOREM (§10.7.8):  
    // In action-angle (rho \= r^2/2, theta), rho\_src \= rho \- R(theta)^2 / 2  
    // is generated by V(theta) \= R(theta)^2 / 2 \=\> det J \= 1 for ANY R(theta)\!  
    float R\_th \= (u\_op \== 0\)  
      ? habitRosette(th, u\_radius, u\_roundness)  
      : habitRhomb(th, u\_radius, u\_roundness);

    if (r \< R\_th) {  
      // Interior coordinate is r / R(theta) so rings are homothetic copies of the habit  
      float ink \= 1.0 \+ u\_parity \+ (r / R\_th);  
      outField \= vec4(0.0, 0.0, ink, u\_aux);  
      return;  
    }  
    float r\_src \= sqrt(r \* r \- R\_th \* R\_th);  
    P\_src \= u\_center \+ vec2(cos(th), sin(th)) \* r\_src;  
  }  
  else if (u\_op \== 2\) {  
    // LAMB-OSEEN SWIRL (§4.3(7)): Exact Twist Map in (rho, theta)  
    float rc \= u\_radius \* 1.2;  
    float r2 \= dot(rel, rel);  
    float angle \= (r2 \< 1e-7)  
      ? (1.8 / (rc \* rc)) \* 0.015  
      : (0.027 / r2) \* (1.0 \- exp(-r2 / (rc \* rc)));  
    float cs \= cos(-angle), sn \= sin(-angle);  
    P\_src \= u\_center \+ vec2(cs \* rel.x \- sn \* rel.y, sn \* rel.x \+ cs \* rel.y);  
  }  
  else if (u\_op \== 3\) {  
    // HAMILTONIAN PINCH (§4.3(5)): Windowed by streamline invariant s \= x\*y \=\> det J \= 1  
    float s \= rel.x \* rel.y;  
    float w \= exp(-abs(s) / (u\_radius \* u\_radius \* 0.35));  
    float k \= 0.55 \* w;  
    P\_src \= u\_center \+ vec2(rel.x \* exp(k), rel.y \* exp(-k));  
  }  
  else if (u\_op \== 4\) {  
    // \[RED CONTROL\] NAIVELY RADIAL-WINDOWED KICK (§10.2 Violation):  
    // Kicking rho with a radial window W(rho) without the F2 angle twist breaks d(omega)=0\!  
    float rho \= 0.5 \* r \* r;  
    float rho0 \= 0.5 \* u\_radius \* u\_radius;  
    float W \= exp(-rho / rho0) \* (rho / rho0);  
    float rho\_src \= max(1e-6, rho \- 0.0045 \* W \* sin(3.0 \* th));  
    float r\_src \= sqrt(2.0 \* rho\_src);  
    P\_src \= u\_center \+ vec2(cos(th), sin(th)) \* r\_src;  
  }

  // Re-base accumulated displacement: d'(P) \= d(P\_src) \+ (P\_src \- P) (libsumi §4.2)  
  vec4 srcSample \= texture(u\_tex, P\_src);  
  vec2 d\_new \= srcSample.xy \+ (P\_src \- P);  
  outField \= vec4(d\_new, srcSample.z, srcSample.w);  
}\`;

// composite.glsl: Kessho Layer A \+ Layer B, Anod, \--kessho-flat (z=1), and Sumi  
const compFsSource \= \`\#version 300 es  
precision highp float;  
in vec2 v\_st;  
out vec4 fragColor;

uniform sampler2D u\_tex;  
uniform int u\_viewMode;  
uniform float u\_polarizer;  
uniform float u\_causticZ;  
uniform float u\_retard;

// Physical Michel-Levy CIE interference approximation for retardation Gamma (nm)  
vec3 michelLevy(float gamma\_nm) {  
  // Integrate sin^2(pi \* Gamma / lambda) across RGB primaries \+ high-order fade  
  vec3 lambda \= vec3(610.0, 550.0, 465.0);  
  vec3 phase \= 3.14159265 \* gamma\_nm / lambda;  
  vec3 s \= sin(phase);  
  vec3 col \= s \* s;  
  float orderFade \= exp(-max(0.0, gamma\_nm \- 1600.0) / 2800.0);  
  vec3 grey \= vec3(dot(col, vec3(0.3, 0.59, 0.11)));  
  col \= mix(grey, col, orderFade);  
  // Darken first 180 nm so resting crystal is dark (§10.7.2)  
  col \*= smoothstep(0.0, 180.0, gamma\_nm);  
  return col;  
}

void main() {  
  float h \= 1.0 / 512.0;  
  vec4 c \= texture(u\_tex, v\_st);  
  vec2 d\_px \= texture(u\_tex, v\_st \+ vec2(h, 0.0)).xy;  
  vec2 d\_mx \= texture(u\_tex, v\_st \- vec2(h, 0.0)).xy;  
  vec2 d\_py \= texture(u\_tex, v\_st \+ vec2(0.0, h)).xy;  
  vec2 d\_my \= texture(u\_tex, v\_st \- vec2(0.0, h)).xy;

  // Jacobian J \= I \+ grad(d)  
  vec2 ddx \= (d\_px \- d\_mx) / (2.0 \* h);  
  vec2 ddy \= (d\_py \- d\_my) / (2.0 \* h);  
  float J11 \= 1.0 \+ ddx.x, J12 \= ddy.x;  
  float J21 \= ddx.y,       J22 \= 1.0 \+ ddy.y;

  float detJ \= J11 \* J22 \- J12 \* J21;  
  float trJ  \= J11 \+ J22;  
  float frob2 \= J11\*J11 \+ J12\*J12 \+ J21\*J21 \+ J22\*J22;

  // Principal stretch sigma \= lambda \- 1/lambda \= sqrt(\|\|J\|\|\_F^2 \- 2\) (§10.7.1)  
  float sigma \= sqrt(max(0.0, frob2 \- 2.0));

  // Principal strain axis psi of C \= J^T J (§10.7.1)  
  float psi \= 0.5 \* atan(  
    2.0 \* (J11 \* J12 \+ J21 \* J22),  
    (J11 \* J11 \+ J21 \* J21) \- (J12 \* J12 \+ J22 \* J22)  
  );

  if (u\_viewMode \== 2\) {  
    // \--kessho-flat (z \= 1\) EXACTNESS DIAGNOSTIC (§10.7.3):  
    // At z \= 1, det(I \+ z\*grad(d)) \= det J. Exact symplectomorphisms have det J \= 1\!  
    float err \= abs(detJ \- 1.0);  
    vec3 okBg \= vec3(0.03, 0.05, 0.07) \+ 0.06 \* vec3(sigma \* 0.3);  
    vec3 badCol \= vec3(1.0, 0.12, 0.35) \* smoothstep(0.008, 0.12, err) \* 2.5;  
    fragColor \= vec4(pow(okBg \+ badCol, vec3(1.0/2.2)), 1.0);  
    return;  
  }

  if (u\_viewMode \== 3\) {  
    // SUMI MEDIUM (§4.5): Washi paper \+ alternating sumi ink rings from c.z  
    vec3 paper \= vec3(0.95, 0.93, 0.88);  
    if (c.z \<= 0.01) {  
      fragColor \= vec4(paper, 1.0);  
      return;  
    }  
    float ring \= 0.5 \+ 0.5 \* cos(c.z \* 3.14159265 \* 6.0);  
    vec3 inkCol \= mix(vec3(0.08, 0.09, 0.12), vec3(0.15, 0.32, 0.55), fract(c.w \* 0.27));  
    vec3 col \= mix(paper, inkCol, smoothstep(0.2, 0.8, ring) \* 0.88);  
    fragColor \= vec4(col, 1.0);  
    return;  
  }

  if (u\_viewMode \== 1\) {  
    // ANOD MEDIUM (§10.4): Strain glow \+ deformed coordinate grid  
    vec3 glass \= vec3(0.012, 0.014, 0.02);  
    float glow \= 0.22 \+ 0.78 \* (1.0 \- exp(-sigma / 0.25));  
    float band \= (c.z \> 0.01) ? (0.55 \+ 0.45 \* cos(c.z \* 18.85)) : 0.0;  
    vec3 charge \= vec3(0.15, 0.65, 1.0) \* glow \* band \* 1.4;  
    vec2 warped \= v\_st \+ vec2(-4.0, 3.4) \* c.xy;  
    vec2 grid \= abs(fract(warped \* 48.0 \- 0.5) \- 0.5) / fwidth(warped \* 48.0);  
    float line \= 1.0 \- min(min(grid.x, grid.y), 1.0);  
    float moved \= smoothstep(0.0005, 0.004, length(c.xy));  
    vec3 col \= glass \+ charge \+ vec3(0.08, 0.22, 0.35) \* line \* moved;  
    fragColor \= vec4(pow(col, vec3(1.0/2.2)), 1.0);  
    return;  
  }

  // KESSHO MEDIUM (§10.7.1 \- §10.7.4):  
  // Substrate: backlit frosted calcite  
  vec3 substrate \= vec3(0.012, 0.013, 0.018);

  // Layer B1: Trace discriminant Delta \= (tr J)^2 \- 4 (Elliptic cool vs Hyperbolic warm)  
  float delta \= trJ \* trJ \- 4.0;  
  float regimeWeight \= smoothstep(0.05, 0.8, abs(delta));  
  vec3 tint \= (delta \< 0.0)  
    ? vec3(0.04, 0.16, 0.32) \* regimeWeight   // Elliptic (Delta \< 0: vortex core)  
    : vec3(0.34, 0.11, 0.04) \* regimeWeight;  // Hyperbolic (Delta \> 0: saddle/pinch)

  // Layer B2: Collapsed Symplectic Lagrangian Caustic (§10.7.3):  
  // det(I \+ z\*grad(d)) \= 1 \+ z\*(1 \- z)\*(tr J \- 2\)  
  float z \= u\_causticZ;  
  float den \= 1.0 \+ z \* (1.0 \- z) \* (trJ \- 2.0);  
  float causticSoft \= max(0.0, 1.0 / (abs(den) \+ 0.08) \- 1.0 / 1.08) \* 0.12;  
  float causticLine \= (1.0 \- smoothstep(0.0, 1.5 \* fwidth(den), abs(den))) \* smoothstep(0.02, 0.15, sigma);  
  vec3 layerB \= tint \+ vec3(0.95, 0.88, 0.72) \* (causticSoft \+ causticLine \* 0.9);

  // Layer A: Photoelastic Birefringence I\_A \= sin^2(2\*(psi \- alpha)) \* ML(Gamma \* sigma)  
  float isoclinic \= pow(sin(2.0 \* (psi \- u\_polarizer)), 2.0);  
  vec3 layerA \= isoclinic \* 0.85 \* michelLevy(u\_retard \* sigma);

  vec3 total \= substrate \+ layerB \+ layerA;  
  fragColor \= vec4(pow(total / (1.0 \+ 0.35 \* total), vec3(1.0 / 2.2)), 1.0);  
}\`;

function createShader(type, src) {  
  const s \= gl.createShader(type);  
  gl.shaderSource(s, src);  
  gl.compileShader(s);  
  if (\!gl.getShaderParameter(s, gl.COMPILE\_STATUS)) console.error(gl.getShaderInfoLog(s));  
  return s;  
}  
function createProg(vs, fs) {  
  const p \= gl.createProgram();  
  gl.attachShader(p, createShader(gl.VERTEX\_SHADER, vs));  
  gl.attachShader(p, createShader(gl.FRAGMENT\_SHADER, fs));  
  gl.linkProgram(p);  
  return p;  
}

const deformProg \= createProg(vsSource, deformFsSource);  
const compProg   \= createProg(vsSource, compFsSource);

const quadBuf \= gl.createBuffer();  
gl.bindBuffer(gl.ARRAY\_BUFFER, quadBuf);  
gl.bufferData(gl.ARRAY\_BUFFER, new Float32Array(\[-1,-1, 3,-1, \-1,3\]), gl.STATIC\_DRAW);

function makeFBO() {  
  const tex \= gl.createTexture();  
  gl.bindTexture(gl.TEXTURE\_2D, tex);  
  gl.texImage2D(gl.TEXTURE\_2D, 0, gl.RGBA32F, SIM\_RES, SIM\_RES, 0, gl.RGBA, gl.FLOAT, null);  
  gl.texParameteri(gl.TEXTURE\_2D, gl.TEXTURE\_MIN\_FILTER, gl.LINEAR);  
  gl.texParameteri(gl.TEXTURE\_2D, gl.TEXTURE\_MAG\_FILTER, gl.LINEAR);  
  gl.texParameteri(gl.TEXTURE\_2D, gl.TEXTURE\_WRAP\_S, gl.CLAMP\_TO\_EDGE);  
  gl.texParameteri(gl.TEXTURE\_2D, gl.TEXTURE\_WRAP\_T, gl.CLAMP\_TO\_EDGE);  
  const fbo \= gl.createFramebuffer();  
  gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);  
  gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR\_ATTACHMENT0, gl.TEXTURE\_2D, tex, 0);  
  return { tex, fbo };  
}

let ping \= makeFBO(), pong \= makeFBO();

function resetField() {  
  gl.bindFramebuffer(gl.FRAMEBUFFER, ping.fbo);  
  gl.viewport(0, 0, SIM\_RES, SIM\_RES);  
  gl.clearColor(0, 0, 0, 0); // Identity displacement d \= (0, 0, 0, 0\)  
  gl.clear(gl.COLOR\_BUFFER\_BIT);  
  dropCounter \= 0;  
}  
resetField();

function applyDeform(op, x, y, radius) {  
  gl.bindFramebuffer(gl.FRAMEBUFFER, pong.fbo);  
  gl.viewport(0, 0, SIM\_RES, SIM\_RES);  
  gl.useProgram(deformProg);

  gl.activeTexture(gl.TEXTURE0);  
  gl.bindTexture(gl.TEXTURE\_2D, ping.tex);  
  gl.uniform1i(gl.getUniformLocation(deformProg, 'u\_tex'), 0);  
  gl.uniform1i(gl.getUniformLocation(deformProg, 'u\_op'), op);  
  gl.uniform2f(gl.getUniformLocation(deformProg, 'u\_center'), x, y);  
  gl.uniform1f(gl.getUniformLocation(deformProg, 'u\_radius'), radius);  
  gl.uniform1f(gl.getUniformLocation(deformProg, 'u\_roundness'), parseFloat(document.getElementById('roundness').value));  
  gl.uniform1f(gl.getUniformLocation(deformProg, 'u\_parity'), (dropCounter % 2));  
  gl.uniform1f(gl.getUniformLocation(deformProg, 'u\_aux'), dropCounter);  
  dropCounter++;

  const loc \= gl.getAttribLocation(deformProg, 'a\_pos');  
  gl.enableVertexAttribArray(loc);  
  gl.bindBuffer(gl.ARRAY\_BUFFER, quadBuf);  
  gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);  
  gl.drawArrays(gl.TRIANGLES, 0, 3);

  let tmp \= ping; ping \= pong; pong \= tmp;  
}

// Seed initial crystal drops \+ swirl so Kessho fringes and Maltese cross appear immediately  
applyDeform(0, 0.38, 0.48, 0.14);  
applyDeform(1, 0.62, 0.52, 0.14);  
applyDeform(2, 0.50, 0.50, 0.22);

canvas.addEventListener('pointerdown', (e) \=\> {  
  const rect \= canvas.getBoundingClientRect();  
  const x \= (e.clientX \- rect.left) / rect.width;  
  const y \= (e.clientY \- rect.top) / rect.height;  
  const op \= parseInt(document.getElementById('opMode').value, 10);  
  applyDeform(op, x, y, 0.13);  
});

document.getElementById('dipBtn').addEventListener('click', resetField);

// \============================================================================  
// PART 2: SUZU PHASE-SPACE AUDIO ENGINE \+ BORIS GYROSCOPE \+ SCOPE OVERLAY  
// \============================================================================  
let audioCtx \= null, scriptNode \= null, audioRunning \= false;  
// 4D Boris / 2D Magic Circle state: (x1, y1, x2, y2)  
let suzu \= { x1: 0.5, y1: 0.0, x2: 0.0, y2: 0.5, eps: 0.05 };  
const traceRing \= new Float32Array(512 \* 2);  
let traceHead \= 0;

function retuneSuzu(newFreq, sampleRate) {  
  const newEps \= 2.0 \* Math.sin(Math.PI \* newFreq / sampleRate);  
  const retuneMode \= parseInt(document.getElementById('retuneMode').value, 10);  
  if (retuneMode \=== 0 && suzu.eps \> 0 && suzu.eps \< 1.99) {  
    // Suzu Cell::retune (§13.2): Re-base (x, y) onto the new eps-ellipse I\_eps \= x^2 \+ y^2 \- eps\*x\*y  
    const sOld \= Math.sqrt(Math.max(1e-6, 1.0 \- 0.25 \* suzu.eps \* suzu.eps));  
    const sNew \= Math.sqrt(Math.max(1e-6, 1.0 \- 0.25 \* newEps \* newEps));  
    const vOrth1 \= (suzu.y1 \- 0.5 \* suzu.eps \* suzu.x1) / sOld;  
    suzu.y1 \= 0.5 \* newEps \* suzu.x1 \+ vOrth1 \* sNew;  
    const vOrth2 \= (suzu.y2 \- 0.5 \* suzu.eps \* suzu.x2) / sOld;  
    suzu.y2 \= 0.5 \* newEps \* suzu.x2 \+ vOrth2 \* sNew;  
  }  
  suzu.eps \= newEps;  
}

document.getElementById('audioBtn').addEventListener('click', () \=\> {  
  if (\!audioCtx) {  
    audioCtx \= new (window.AudioContext \|\| window.webkitAudioContext)();  
    suzu.eps \= 2.0 \* Math.sin(Math.PI \* 220 / audioCtx.sampleRate);  
    suzu.x1 \= 0.5; suzu.y1 \= 0.25 \* suzu.eps;  
    suzu.x2 \= 0.0; suzu.y2 \= 0.35;

    scriptNode \= audioCtx.createScriptProcessor(512, 0, 2);  
    scriptNode.onaudioprocess \= (e) \=\> {  
      const L \= e.outputBuffer.getChannelData(0);  
      const R \= e.outputBuffer.getChannelData(1);  
      const mode \= parseInt(document.getElementById('suzuMode').value, 10);  
      const B \= parseFloat(document.getElementById('magB').value);  
      const dt \= 1.0 / audioCtx.sampleRate;  
      // Prewarped Cayley magnetic rotation angle theta\_B \= 2 \* atan(B \* dt / 2\) (§13.13)  
      const thB \= 2.0 \* Math.atan(0.5 \* B \* 2.0 \* Math.PI \* dt);  
      const cB \= Math.cos(thB), sB \= Math.sin(thB);  
      const eps \= suzu.eps;

      for (let i \= 0; i \< L.length; i++) {  
        if (mode \=== 1) {  
          // \[RED CONTROL\] Naive Simultaneous Euler: det J \= 1 \+ eps^2 \> 1 (Blows up\!)  
          const nx1 \= suzu.x1 \- eps \* suzu.y1;  
          const ny1 \= suzu.y1 \+ eps \* suzu.x1;  
          suzu.x1 \= Math.max(-2.0, Math.min(2.0, nx1));  
          suzu.y1 \= Math.max(-2.0, Math.min(2.0, ny1));  
        } else {  
          // SYMPLECTIC MAGIC CIRCLE \+ BORIS CAYLEY ROTATION (det J \= 1 identically\!)  
          suzu.x1 \-= eps \* suzu.y1;  
          suzu.y1 \+= eps \* suzu.x1;  
          suzu.x2 \-= eps \* suzu.y2;  
          suzu.y2 \+= eps \* suzu.x2;  
          if (B \> 0.0) {  
            // Twisted 2-form omega \= dp^dx \+ B dx1^dx2 \-\> Exact Cayley rotation in (1, 2\) plane  
            const rx \= cB \* suzu.x1 \- sB \* suzu.x2;  
            const ry \= sB \* suzu.x1 \+ cB \* suzu.x2;  
            const px \= cB \* suzu.y1 \- sB \* suzu.y2;  
            const py \= sB \* suzu.y1 \+ cB \* suzu.y2;  
            suzu.x1 \= rx; suzu.x2 \= ry;  
            suzu.y1 \= px; suzu.y2 \= py;  
          }  
        }  
        L\[i\] \= suzu.x1 \* 0.22;  
        R\[i\] \= (B \> 0.0 ? suzu.x2 : suzu.y1) \* 0.22;

        if ((i & 7) \=== 0) {  
          traceRing\[traceHead \* 2\]     \= suzu.x1;  
          traceRing\[traceHead \* 2 \+ 1\] \= (B \> 0.0) ? suzu.x2 : suzu.y1;  
          traceHead \= (traceHead \+ 1) & 511;  
        }  
      }  
    };  
    scriptNode.connect(audioCtx.destination);  
    audioRunning \= true;  
    document.getElementById('audioBtn').textContent \= 'Mute Suzu Audio';  
  } else if (audioRunning) {  
    audioCtx.suspend();  
    audioRunning \= false;  
    document.getElementById('audioBtn').textContent \= 'Resume Suzu Audio';  
  } else {  
    audioCtx.resume();  
    audioRunning \= true;  
    document.getElementById('audioBtn').textContent \= 'Mute Suzu Audio';  
  }  
});

document.getElementById('freq').addEventListener('input', (e) \=\> {  
  const f \= parseFloat(e.target.value);  
  document.getElementById('freqVal').textContent \= f;  
  if (audioCtx) retuneSuzu(f, audioCtx.sampleRate);  
});  
document.getElementById('pluckBtn').addEventListener('click', () \=\> {  
  suzu.x1 \= 0.5; suzu.y1 \= 0.25 \* suzu.eps;  
  suzu.x2 \= 0.0; suzu.y2 \= 0.35;  
});

// UI Value Labels  
\['polarizer', 'causticZ', 'retard', 'roundness', 'magB'\].forEach(id \=\> {  
  const el \= document.getElementById(id);  
  el.addEventListener('input', () \=\> {  
    document.getElementById(id \+ 'Val').textContent \= parseFloat(el.value).toFixed(id \=== 'retard' ? 0 : 2);  
  });  
});

// \============================================================================  
// RENDER LOOP \+ sumi\_set\_scope LIVE PHASE-SPACE OVERLAY  
// \============================================================================  
const scopeCanvas \= document.getElementById('scopecanvas');  
const sctx \= scopeCanvas.getContext('2d');

function render() {  
  gl.bindFramebuffer(gl.FRAMEBUFFER, null);  
  gl.viewport(0, 0, canvas.width, canvas.height);  
  gl.useProgram(compProg);

  gl.activeTexture(gl.TEXTURE0);  
  gl.bindTexture(gl.TEXTURE\_2D, ping.tex);  
  gl.uniform1i(gl.getUniformLocation(compProg, 'u\_tex'), 0);  
  gl.uniform1i(gl.getUniformLocation(compProg, 'u\_viewMode'), parseInt(document.getElementById('viewMode').value, 10));  
  gl.uniform1f(gl.getUniformLocation(compProg, 'u\_polarizer'), parseFloat(document.getElementById('polarizer').value));  
  gl.uniform1f(gl.getUniformLocation(compProg, 'u\_causticZ'), parseFloat(document.getElementById('causticZ').value));  
  gl.uniform1f(gl.getUniformLocation(compProg, 'u\_retard'), parseFloat(document.getElementById('retard').value));

  const loc \= gl.getAttribLocation(compProg, 'a\_pos');  
  gl.enableVertexAttribArray(loc);  
  gl.bindBuffer(gl.ARRAY\_BUFFER, quadBuf);  
  gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);  
  gl.drawArrays(gl.TRIANGLES, 0, 3);

  // Draw sumi\_set\_scope live overlay (libsumi 1.3.0 §13.2)  
  sctx.clearRect(0, 0, scopeCanvas.width, scopeCanvas.height);  
  if (audioRunning) {  
    const cx \= scopeCanvas.width \* 0.82, cy \= scopeCanvas.height \* 0.18, scale \= 95;  
    sctx.strokeStyle \= 'rgba(56, 189, 248, 0.25)';  
    sctx.beginPath(); sctx.arc(cx, cy, scale \* 0.5, 0, Math.PI \* 2); sctx.stroke();

    sctx.strokeStyle \= '\#38bdf8';  
    sctx.lineWidth \= 1.6;  
    sctx.beginPath();  
    for (let i \= 0; i \< 512; i++) {  
      const idx \= (traceHead \+ i) & 511;  
      const px \= cx \+ traceRing\[idx \* 2\] \* scale;  
      const py \= cy \- traceRing\[idx \* 2 \+ 1\] \* scale;  
      if (i \=== 0) sctx.moveTo(px, py); else sctx.lineTo(px, py);  
    }  
    sctx.stroke();

    const I\_eps \= suzu.x1 \* suzu.x1 \+ suzu.y1 \* suzu.y1 \- suzu.eps \* suzu.x1 \* suzu.y1;  
    document.getElementById('hudStatus').textContent \=  
      \`Suzu Invariant I\_ε \= \${I\_eps.toFixed(5)} \| det J \= \${document.getElementById('suzuMode').value \=== '0' ? '1.000000' : (1 \+ suzu.eps\*suzu.eps).toFixed(6)}\`;  
  }  
  requestAnimationFrame(render);  
}  
requestAnimationFrame(render);  
\</script\>  
\</body\>  
\</html\>

### **Module 1 Problem Set (Connecting Theory to midi-sink Code)**

> 1. **Problem 1.1 (The Collapsed Kessho Caustic Identity, §10.7.3):** Let $d(x,y)=(u-x,v-y)$ be the displacement field stored in libsumi, with Jacobian \$J \= I \+ \\nabla\\mathbf{d}\$.  
   * (a) Prove that for *any* differentiable 2D map, \$\\det(I \+ z\\nabla\\mathbf{d}) \= (1-z)^2 \+ z(1-z)\\operatorname{tr} J \+ z^2 \\det J\$.  
   * (b) Show that if the deformation chain consists exclusively of exact symplectomorphisms ($\det J=1$), this simplifies to Kessho’s shader formula \$\\det(I \+ z\\nabla\\mathbf{d}) \= 1 \+ z(1-z)(\\operatorname{tr} J \- 2)\$.  
   * (c) Explain why a vortex with a rigid core ($J={R}_{\phi }$, $\lambda =1$) produces zero birefringence in Kessho Layer A ($\sigma =\sqrt{\parallel J\parallel {}_{F}^{2}-2}=0$) yet still illuminates Kessho Layer B via \$\\Delta \= (\\operatorname{tr} J)^2 \- 4 \= 4(\\cos^2\\phi \- 1\) \< 0\$.  
> 2. **Problem 1.2 (The Symplectic Crystal-Drop Theorem & Cleavage Creases, §10.7.8):** In SUMI\_MEDIUM\_KESSHO, a crystal drop of angular profile $R(\theta )>0$ has inverse lookup ${r}_{src}=\sqrt{{r}^{2}-R(\theta {)}^{2}}$ along each ray $\theta$ for $r\geq R(\theta )$.  
   * (a) Using canonical polar coordinates $\rho ={r}^{2}/2$ and $\theta$, compute the exterior derivative of the pullback ${\Phi }^{*}(d\rho \wedge d\theta )$ and verify that $\det J=1$ holds for any piecewise\-${C}^{1}$ profile $R(\theta )$.  
   * (b) Compute the off-diagonal Jacobian entry $\frac{\partial {\rho }_{src}}{\partial \theta }=-R(\theta )R'(\theta )$. When habit\_roundness $=0$ on the Calcite Rhomb (SUMI\_HABIT\_RHOMB), $R'(\theta )$ has a jump discontinuity at the four vertex angles ${\theta }_{k}$. Show that this jump produces a ray of discontinuous shear strain $\sigma$ extending from each vertex to infinity.  
> 3. **Problem 1.3 (Suzu’s Skewed Invariant & MPE Retuning, §13.2):** Consider Suzu’s Magic Circle recurrence ${x}_{n+1}={x}_{n}-\varepsilon {y}_{n}$, ${y}_{n+1}={y}_{n}+\varepsilon {x}_{n+1}$.  
   * (a) Substitute $({x}_{n+1},{y}_{n+1})$ into ${I}_{\varepsilon }(x,y)={x}^{2}+{y}^{2}-\varepsilon xy$ and prove algebraically that ${I}_{\varepsilon }({x}_{n+1},{y}_{n+1})={I}_{\varepsilon }({x}_{n},{y}_{n})$ for all $0<\varepsilon <2$.  
   * (b) Show that Cell::retune’s transformation \$(x, y) \\mapsto (x, y\_{\\text{new}})\$ preserves the canonical area form $dx\wedge dy$ up to the ratio $\sqrt{\frac{4-(\varepsilon '{)}^{2}}{4-{\varepsilon }^{2}}}$ while mapping the level curve ${I}_{\varepsilon }(x,y)={A}^{2}(1-{\varepsilon }^{2}/4)$ bijectively onto ${I}_{\varepsilon '}(x,{y}_{new})={A}^{2}(1-(\varepsilon '{)}^{2}/4)$.

