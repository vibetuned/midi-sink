William Rowan Hamilton originally invented Hamiltonian mechanics and symplectic geometry in 1832 not for fluid mechanics, but for **crystal optics**—specifically to predict how light rays bend and split inside anisotropic minerals. Because optical ray transport through a crystal is governed by Hamilton's equations on phase space, **Calcite**, **Muscovite**, and **Barite** fit into libsumi and Suzu with zero compromise to the $\det J=1$ invariant.  
Even better, there is a hidden mathematical property of the circular drop equation in §4.3(1) that unlocks **exact crystalline geometry** on the canvas.

### **1\. The Symplectic Crystal Drop Theorem ($\det J=1$ for Any Crystal Habit)**

In §4.3(1), a circular drop of constant radius ${r}_{0}$ centered at $C$ uses the inverse lookup ${P}_{src}=C+(P-C)\sqrt{1-{r}_{0}^{2}/{r}^{2}}$. In canonical polar Darboux coordinates $(\rho =\frac{1}{2}{r}^{2},\theta )$, where the area 2-form is $dx\wedge dy=d\rho \wedge d\theta$, that circular expansion is simply the translation ${\rho }_{src}=\rho -\frac{1}{2}{r}_{0}^{2},{\theta }_{src}=\theta$.  
If we replace the constant radius ${r}_{0}$ with **any angular crystal polygon or rosette profile $R(\theta )$**:

$${P}_{src}=C+(P-C)\sqrt{1-\frac{R(\theta {)}^{2}}{\parallel P-C\parallel {}^{2}}}for\parallel P-C\parallel \geq R(\theta )$$

the exterior differential of the area form is:

\$\$d\\rho\_{\\text{src}} \\wedge d\\theta\_{\\text{src}} \= d\\\!\\left(\\rho \- \\frac{1}{2}R(\\theta)^2\\right) \\wedge d\\theta \= d\\rho \\wedge d\\theta \- R(\\theta)R'(\\theta)\\,\\underbrace{d\\theta \\wedge d\\theta}\_{0} \= d\\rho \\wedge d\\theta\$\$  
Because $d\theta \wedge d\theta =0$ identically, $\det J=1$ **holds exactly for any star-shaped crystal profile $R(\theta )$ at any strike amplitude, with zero sub-stepping**. By regularizing the polygon corners with a crystal-habit roundness parameter, notes can stamp exact **rhombohedral Calcite spars**, **pseudo-hexagonal Muscovite sheets**, or **tabular Barite rosettes**, and every existing operator (Chladni, torsion, pinch, swirl) will deform their facets without losing a single texel of area.

### **2\. The Three Minerals as Symplectic Operators & Kessho Shaders**

Within the new SUMI\_MEDIUM\_KESSHO medium, we can expose a kessho\_mineral selector (0 \= Calcite, 1 \= Muscovite, 2 \= Barite) that pairs each mineral's exact optical law in composite.glsl with its mechanical cleavage law in deform.glsl:

#### **A. Calcite ($CaC{O}_{3}$ — Uniaxial Iceland Spar): Double Refraction & Twin Kink Bands**

> **Optical Shader (Walk-Off Double Refraction):** Calcite is a trigonal uniaxial crystal famous for splitting a single image into two orthogonally polarized rays: the **ordinary ray ($o$\-ray)** and the **extraordinary ray ($e$\-ray)**. In composite.glsl, we sample the displacement field twice—once at the ordinary coordinate ${u}_{o}(x)$ and once shifted along the local principal strain eigenvector ${\hat{e}}_{\psi }$ by the Poynting walk-off vector $w={\delta }_{spar}\sin (2\psi ){\hat{e}}_{\psi }$:

> $${I}_{calcite}={\cos }^{2}(\psi -\alpha )I({u}_{o})+{\sin }^{2}(\psi -\alpha )I({u}_{o}+w)$$

> 1.   
>    Every ink ring, filament, and caustic splits into a crystalline **ghosted double-image** whose separation widens with strain and cross-fades as the player rotates the polarizer $\alpha$ (via CC 74 / Slide).  
> 2. **Symplectic Deformation (Baumhauer Twin Glide):**  
>    When calcite is stressed mechanically, it does not bend smoothly like water; it snaps into **mechanical twin lamellae** via simple shear along its $7{8}^{\circ }/10{2}^{\circ }$ rhombohedral cleavage planes. Using a piecewise-linear ramp shear in the rotated cleavage frame $({x}_{\theta },{y}_{\theta })$:  
>    \$\$x\_{\\theta,\\text{src}} \= x\_\\theta \- \\gamma \\, \\operatorname{ramp}(y\_\\theta), \\quad y\_{\\theta,\\text{src}} \= y\_\\theta \\quad (\\text{Class: Exact, } \\det J \= 1)\$\$  
>    Replacing or layering the smooth §4.3(2) glide tine with a twin-kink shear causes pitch bends to fracture circular drops into sharp, stepped geometric crystal facets.

#### **B. Muscovite (Mica — Biaxial Phyllosilicate): Cassini Lemniscates & Basal Sheet Delamination**

> **Optical Shader (Biaxial Lemniscates & Fabry–Pérot Pearlescence):** Unlike uniaxial Calcite (which forms a single 4-armed Maltese cross), Muscovite is a **biaxial crystal** with two optic axes separated by angle $2V\approx 4{0}^{\circ }$. In composite.glsl, the phase retardance depends on the product of angles to both optic axes, transforming vortex cores (shot\_chladni.jpg, shot\_torsion.jpg) into **figure-eight Cassini ovals (Bernoulli lemniscates)** and curved hyperbolic isogyres that sweep across the eddies. Simultaneously, because Muscovite cleaves into sub-micron transparent sheets, we read the stored ink phase channel ($\in [1,3)$ per §4.2) as **lamellar sheet thickness $h$** in Airy's thin-film Fabry–Pérot equation:

> $${R}_{mica}(\Lambda )=\frac{4{r}^{2}{\sin }^{2}(\delta /2)}{(1-{r}^{2}{)}^{2}+4{r}^{2}{\sin }^{2}(\delta /2)},\delta =\frac{4\pi {n}_{mica}h\cos \theta }{{\Lambda }_{RGB}}$$

> 1.   
>    This gives the bands a realistic **silvery-champagne pearlescent luster** that shifts hue as strain tilts the local sheet normal $\theta$.

> **Symplectic Deformation (Phyllosilicate Terrace Slip):**  
> Mica deforms by sliding discrete basal sheets over one another like a deck of cards. A quantized staircase shear:

> $${x}_{src}=x-A\left({ky-\frac{\sin (2\pi Nky)}{2\pi N}}\right),{y}_{src}=y(Class:Exact,\det J=1)$$

> 2.   
>    concentrates all shear into thin inter-sheet slip planes while keeping the terrace interiors flat, slicing drops into shimmering, delaminated mica flakes when driven by MPE Slide or Ripple.

#### **C. Barite ($BaS{O}_{4}$ — Orthorhombic Biaxial): Hamilton's Conical Refraction & Desert Rose Rosettes**

> 1. **Optical Shader (Hamilton's Conical Refraction & Prismatic Dispersion):** Hamilton's 1832 triumph was proving that when a ray aligns with the optic axis of a biaxial crystal like Barite, the wave surface forms a **diabolical conical point**: a single ray opens into a **hollow luminous ring**. In composite.glsl, wherever the Jacobian's trace discriminant approaches the parabolic horizon (\$\\vert{}\\operatorname{tr} J\\vert{} \\approx 2\$), we sample a miniature conical ring around the pre-image and apply **Cauchy chromatic dispersion** (${z}_{R}<{z}_{G}<{z}_{B}$) to the caustic determinant \$\\det(I \+ z\_{\\text{RGB}}\\nabla\\mathbf{d})^{-1}\$. High-strain ridges burst into sharp **prismatic rainbow fire** and hollow Hamiltonian rings.  
> 2. **Symplectic Deformation (Orthorhombic "Desert Rose" Rosettes):**  
>    Barite crystallizes in tabular orthorhombic blades ($001$ and $210$ faces intersecting at $78.{5}^{\circ }$ and $101.{5}^{\circ }$) that cluster into radial **Desert Rose** rosettes. Using the Symplectic Crystal Drop equation above with an $m$\-bladed orthorhombic envelope:  
>    \$\$R(\\theta) \= r\_0 \\left( 1 \+ \\beta \\sum\_{j=1}^{m} \\cos^{2p}\\\!\\big(m(\\theta \- \\theta\_j)\\big) \\right)\$\$  
>    each note strike or sustained MPE press (§3.4 boundary growth) grows a sharp, interlocked rosette of tabular crystal blades whose area grows linearly with pressure while pushing surrounding bands outward without distortion.  
> 3. **Suzu Acoustic Counterpart (Diabolical Point Mode Splitting):** In Suzu's modal lattice (§13.2), Hamilton's conical refraction corresponds to a **2D conical intersection (Dirac point)** between two degenerate normal modes coupled by two parameters (MPE Press $p$ and Slide $t$):  
>    \$\$H\_{\\text{conical}} \= \\begin{pmatrix} \\omega\_0 \+ \\kappa\_1 p & \\kappa\_2 t \\\\ \\kappa\_2 t & \\omega\_0 \- \\kappa\_1 p \\end{pmatrix} \\implies \\omega\_\\pm \= \\omega\_0 \\pm \\sqrt{(\\kappa\_1 p)^2 \+ (\\kappa\_2 t)^2}\$\$  
>    Circling the origin in $(p,t)$ MPE expression space carries the audio oscillator around the diabolical point, imparting a **geometric Berry phase ($\pi$ phase flip)** to the sound wave and drawing a double-cone hourglass in the trace/ overlay.

### **3\. Summary of the Three Mineral Sub-Modes in Kessho**

| Mineral Mode (kessho\_mineral) | Crystal System | composite.glsl Symplectic Optical Law MD | deform.glsl Exact (det⁡J=1\\det J \= 1detJ=1) Operator MD | Suzu Phase-Space Twin (§13) MD |
| :---- | :---- | :---- | :---- | :---- |
| **0: Calcite** (Iceland Spar) | Trigonal (Uniaxial $-$ ) | **Walk-off double refraction** (${u}_{o}$ vs. ${u}_{e}$ split along strain axis ${\hat{e}}_{\psi }$) \+ 4-arm Maltese cross | **Rhombohedral crystal drop** ($7{8}^{\circ }/10{2}^{\circ }$) \+ **Baumhauer twin-kink shear** | Birefringent dual-delay hybrid string (orthogonal $o/e$ polarization loops) |
| **1: Muscovite** (Mica) | Monoclinic (Biaxial $-$) | **Cassini oval / lemniscate isogyres** ($2V$ two-axis product) \+ **Airy thin-film pearlescence** | **Pseudo-hexagonal drop** \+ **Basal terrace slip** (staircase delamination shear) | Coupled layered sheet lattice (weak inter-layer Van der Waals springs) |
| **2: Barite** (Baryte) | Orthorhombic (Biaxial $+$) | **Hamilton's conical refraction rings** at \$\\Vert{}\\operatorname{tr} J\\Vert{} \= 2\$ \+ **RGB Cauchy dispersion fire** | **Tabular "Desert Rose" rosette drop** (exact polar Darboux shear $d\rho \wedge d\theta$) | **Diabolical conical intersection** (Berry phase $\pi$ flip around $(p,t)$ MPE loop) |

### **4\. Drop-in Update to Step 87 of the Roadmap**

To fold all three minerals into the v3.0 roadmap, **Step 87** simply expands its scope by two parameters (kessho\_mineral and crystal\_habit in sumi\_params\_t):

* **Step 87 (Updated) — Medium 2: Kessho (Calcite, Muscovite & Barite) \+ Symplectic Crystal Drops:**  
  * **Core (deform.glsl):** Add the exact polar Darboux crystal habit profile $R(\theta )$ to SUMI\_DEFORM\_DROP (0 \= circle, 1 \= Calcite rhomb, 2 \= Muscovite hex-sheet, 3 \= Barite tabular rosette) and the exact Baumhauer twin-kink / Muscovite terrace-slip profiles to SUMI\_DEFORM\_SPARK / TINE.  
  * **Composite (composite.glsl):** Implement SUMI\_MEDIUM\_KESSHO \= 2 with kessho\_mineral switching between **Calcite** (uniaxial Maltese cross \+ double-refraction walk-off lookup), **Muscovite** (biaxial $2V$ Cassini lemniscates \+ Airy thin-film pearlescent interference), and **Barite** (conical refraction rings \+ RGB Cauchy caustic dispersion).  
  * **Acceptance Gates:** \--soak crystal\_drop and \--soak terrace\_shear pass all four §10.2 exact conservation gates ($\det J=1$ analytically, $\pm k$ inversion, zero fabrication); live switching between Sumi, Anod, and Kessho (all 3 minerals) is bitwise on the field.