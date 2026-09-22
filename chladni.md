Unlike the Faraday effect (which requires fluid viscosity and fails in a vacuum), classic Chladni figures form purely through mechanical inertia and gravity. In fact, sand patterns become *sharper* in a vacuum because aerodynamic drag is eliminated.

By using high-frequency vibrational averaging (the Kapitza/ponderomotive approach), the driving mechanism of classic Chladni motion can be written as an **exact, conservative Hamiltonian system** with an associated **symplectic kick-drift map**.

### **1\. The Ponderomotive Hamiltonian (Secular Motion)**

Consider a plate with vibration mode $Z(x, y, t) \= W(x, y)\\cos(\\omega t)$. A grain of sand on the surface experiences a vertical normal force from the vibrating plate:

&nbsp;

$$N(t) \\approx m\\left(g \- \\ddot{Z}\\right) \= m\\left(g \+ \\omega^2 W(x, y)\\cos(\\omega t)\\right)$$

Because the plate flexes, its local surface normal tilts by an angle $\\nabla Z$. The horizontal force pushing the particle along the plate is:

&nbsp;

$$\\mathbf{F}\_{\\parallel}(t) \= \-N(t) \\nabla Z(x, y, t) \= \-m\\left(g \+ \\omega^2 W \\cos(\\omega t)\\right) \\nabla W \\cos(\\omega t)$$

Averaging this force over one fast oscillation period $T \= \\frac{2\\pi}{\\omega}$:

* $\\langle \\cos(\\omega t) \\rangle \= 0$  
* $\\langle \\cos^2(\\omega t) \\rangle \= \\frac{1}{2}$

The time-averaged secular force becomes:

&nbsp;

$$\\langle \\mathbf{F}\_{\\parallel} \\rangle \= \-\\frac{1}{2} m \\omega^2 W(x, y) \\nabla W(x, y) \= \-\\nabla \\left( \\frac{1}{4} m \\omega^2 W(x, y)^2 \\right)$$

This force is the negative gradient of an **effective potential energy**:

&nbsp;

$$V\_{\\text{eff}}(x, y) \= \\frac{1}{4} m \\omega^2 W(x, y)^2$$

The slow, secular motion of the sand grain is governed by the time-independent canonical Hamiltonian:

&nbsp;

$$H\_{\\text{eff}}(x, y, p\_x, p\_y) \= \\frac{p\_x^2 \+ p\_y^2}{2m} \+ \\frac{1}{4} m \\omega^2 W(x, y)^2$$

Because $W(x, y) \= 0$ defines the nodal lines, **the nodal lines are the absolute global potential minima ($V\_{\\text{eff}} \= 0$)**. The antinodes are potential peaks. Sand is driven downhill toward the nodal valleys.

### **2\. The Symplectic Kick-Drift Map**

Because $H\_{\\text{eff}}$ is separable ($H \= T(\\mathbf{p}) \+ V(\\mathbf{x})$), it can be integrated using an explicit, exactly symplectic kick-drift operator (such as Verlet or Leapfrog splitting):

&nbsp;

$$\\mathbf{p}\_{n+1} \= \\mathbf{p}\_n \- \\Delta t \\, \\left\[\\frac{1}{2} m \\omega^2 W(\\mathbf{x}\_n) \\nabla W(\\mathbf{x}\_n)\\right\] \\quad \\text{(Kick)}$$

&nbsp;

$$\\mathbf{x}\_{n+1} \= \\mathbf{x}\_n \+ \\frac{\\Delta t}{m} \\mathbf{p}\_{n+1} \\quad \\text{(Drift)}$$

#### **Symplectic Verification**

In 1D $(x, p)$, the Jacobian of this step is:

&nbsp;

$$J \= \\begin{pmatrix} 1 & \\frac{\\Delta t}{m} \\\\ \-\\Delta t V''\_{\\text{eff}}(x) & 1 \- \\frac{\\Delta t^2}{m} V''\_{\\text{eff}}(x) \\end{pmatrix}$$

&nbsp;

$$\\det(J) \= 1 \\cdot \\left(1 \- \\frac{\\Delta t^2}{m}V''\\right) \- \\left(-\\frac{\\Delta t^2}{m}V''\\right) \\equiv 1$$

Phase-space volume $dx \\wedge dp\_x \+ dy \\wedge dp\_y$ is strictly conserved.

### **3\. The Full 3D Bouncing Billiard Map**

If grains leave the plate entirely (ballistic hopping when $\\omega^2 \\vert{}W\\vert{} \> g$), the system can be modeled as a **gravitational billiard with a moving boundary**:

> 1. **Free Flight (Drift):** Pure Hamiltonian motion in gravity ($H \= \\frac{\\vert{}\\mathbf{p}\\vert{}^2}{2m} \+ mgz$).  
> 2. **Impact (Kick):** At the collision time $t\_c$ where $z(t\_c) \= W(x, y)\\cos(\\omega t\_c)$, an elastic collision ($e \= 1$) reflects the particle momentum relative to the local moving boundary normal $\\mathbf{n}$:  
>    $$\\mathbf{v}^+ \= \\mathbf{v}^- \- 2\\left\[(\\mathbf{v}^- \- \\mathbf{v}\_{\\text{plate}}) \\cdot \\mathbf{n}\\right\]\\mathbf{n}$$

The Poincaré impact map $(x\_n, y\_n, p\_{x,n}, p\_{y,n}, t\_n)$ of this moving billiard is **strictly symplectic**.

* At antinodes, the boundary oscillates rapidly, injecting kinetic energy into the vertical degree of freedom through chaotic Fermi acceleration.  
* At nodal lines, $\\mathbf{v}\_{\\text{plate}} \= 0$. Grains hitting near a node receive no vertical kick and decouple from the violent vertical excitation.

### **The Boundary Between Symplectic Theory and Reality**

| Mechanism | Symplectic Formulation | Real Physical Behavior |
| :---- | :---- | :---- |
| **Motion towards nodes** | Driven by conservative potential gradient $-\\nabla V\_{\\text{eff}}$ | Driven by the same $-\\nabla V\_{\\text{eff}}$ force |
| **At the nodal line** | Grains oscillate back and forth forever across the trough (orbiting the minimum) | Grains stop and settle into sharp lines due to inelastic collisions ($e \< 1$) and surface friction |

The conservative force that organizes the pattern is fully symplectic. Physical dissipation is only required for the grains to lose their residual energy and come to rest at the bottom of the potential valleys.