With the v2.0 arc folded at Step 70, the v3.0 arc opens **\_work/ROADMAP\_6.md** and **\_work/DECISIONS\_10.md** starting at **Step 71** (Phases 11–15).  
Below is the architectural specification for the three pillars you requested—**(1) the deferred §13.7 Suzu items led by the 2D FDTD membrane/gong**, **(2) new symplectic geometry synthesis models for Suzu**, and **(3) the §12 Voxo Decent Sampler expansion**—followed by the step-by-step **v3.0 Roadmap**.

## **1\. Suzu (§13.7 Deferred): 2D FDTD Membranes, Gongs & Physical Extensions**

### **1.1 2D Symplectic FDTD Membranes, Plates & Von Kármán Gongs**

The §13.7 spec notes that a 32×32 drumhead requires \~50M node-updates/s per voice at the 2× oversampled rate, with the gong's nonlinearity multiplying that cost. We can solve both the physics and the CPU budget without breaking Voxo's audio-thread contract (§12.1) by separating the linear grid dispersion from a symplectic potential kick, and offering two execution topologies:  
**The Symplectic Leapfrog Grid (Symplectic \+ Conformal class):** On an ${N}_{x}\times {N}_{y}$ grid with displacement ${u}_{i,j}$ and momentum ${p}_{i,j}$, the linear membrane/plate Hamiltonian is \$H\_0 \= \\sum\_{i,j} \\left\[ \\frac{1}{2} p\_{i,j}^2 \+ \\frac{c^2}{2} \\vert{}\\nabla\_h u\_{i,j}\\vert{}^2 \+ \\frac{\\kappa^2}{2} (\\Delta\_h u\_{i,j})^2 \\right\]\$, where $c$ is tension wave speed and $\kappa$ is biharmonic plate stiffness. The drift–kick leapfrog step is $\det J=1$ exactly:

$${p}_{i,j}^{n+1/2}={p}_{i,j}^{n-1/2}+\Delta t\left({{c}^{2}{\Delta }_{h}{u}_{i,j}^{n}-{\kappa }^{2}{\Delta }_{h}^{2}{u}_{i,j}^{n}}\right)$$

$${u}_{i,j}^{n+1}={u}_{i,j}^{n}+\Delta t{p}_{i,j}^{n+1/2}$$

*   
  Followed by the declared conformal contraction \$p\_{i,j} \\mathrel{\\times}= \\gamma\_0 \- \\gamma\_2 \\Delta\_h\$ (frequency-dependent air and thermoelastic loss).

**The CFL Gate:**  
For the 5-point Laplacian \+ 13-point biharmonic stencil on a square grid of spacing $h$, the stability limit is:

$${\lambda }_{2D}=\Delta t\sqrt{\frac{8{c}^{2}}{{h}^{2}}+\frac{64{\kappa }^{2}}{{h}^{4}}}\leq 2$$

*   
  Just as the 1D Verlet chain reduces $M$ per note (§13.2), the 2D membrane scales its active grid dimensions $({N}_{x},{N}_{y})$ per note so the fundamental $(1,1)$ mode lands on ${f}_{0}$ to $0.000cent$ while keeping ${\lambda }_{2D}\leq 1.95$.  
* **The Gong Crash (Berger / Von Kármán Symplectic Shear):**  
  Solving full von Kármán Airy stress tensors per sample is prohibitive. Instead, we use the **Berger invariant approximation**, where mid-plane stretching depends on the global mean squared gradient \$E\_{\\text{stretch}}(u) \= \\sum\_{i,j} \\vert{}\\nabla\_h u\_{i,j}\\vert{}^2\$:  
  \$\$H\_{\\text{gong}}(u) \= \\frac{\\alpha}{4} \\left( \\sum\_{i,j} \\vert{}\\nabla\_h u\_{i,j}\\vert{}^2 \\right)^2 \\implies \\Delta p\_{i,j} \= \-\\Delta t \\, \\alpha \\, E\_{\\text{stretch}}(u) \\left( \-\\Delta\_h u\_{i,j} \\right)\$\$  
  Because ${E}_{stretch}(u)$ is a scalar computed once per sub-step during the existing ${\Delta }_{h}u$ pass and depends *only on position $u$*, the nonlinear update is an **exact momentum shear** ($\det J=1$ in $2{N}_{x}{N}_{y}$\-dimensional phase space). It costs one multiply-add per node, never blows up below the CFL ceiling, and produces the classic tam-tam pitch swell and energy cascade into high modes on hard strikes.  
* **Dual Topology (Solving the §13.7 CPU Budget):**  
  1. **Per-Voice SIMD Mini-Membrane (16×16 cap):** Using the carried Structure-of-Arrays (SoA) SIMD layout (§13.7), 16 float columns fit into two AVX2 registers (\_\_m256) or four NEON registers (float32x4\_t). Horizontal neighbors become in-register shifts, cutting per-voice cost by \~6×.  
  2. **Shared Bus Plate / Canvas Resonator (64×64 single instance):** A single large plate lives on the Voxo bus. Every MPE voice strikes or bows the plate at the exact normalized $(x,y)$ coordinate returned by sumi\_layout\_probe (§5.3)—turning the visual marbling canvas into a literal shared acoustic soundboard.

### **1.2 Other §13.7 Deferred Physical Upgrades**

* **Physical Stick-Slip (Helmholtz) Bowing:** Replaces/augments the §13.2 proportional energy servo with an implicit hyperbolic friction curve $\phi ({v}_{rel})={\mu }_{d}+({\mu }_{s}-{\mu }_{d})\frac{{v}_{0}^{2}}{{v}_{0}^{2}+{v}_{rel}^{2}}$ solved by Newton-Raphson in 3 iterations against the string/mode impedance, bounded by the wind engine's power-limited port discipline (${E}_{injected}\leq {F}_{bow}\cdot {v}_{bow}\cdot \Delta t$).  
* **Bore v2 (Toneholes, Two-Mass Lips, Shock Steepening):**  
  * **Toneholes as Branch Ports:** Shunt acoustic inertance \+ radiation resistance branches along the Webster Yee grid (§13.2) controlled by the Phase 9 fingering CCs (CC 110–112).  
  * **Nonlinear Bore Shock Capture:** Adds the §13.7 $(\gamma +1)/2$ convective wave-steepening shear along the bore via an energy-conserving flux limiter so high-breath brass notes develop authentic harmonic edge along the tube, not just at the final bell cell.  
  * **Two-Mass Lips & Reed Flow Inertia:** Upgrades the 1-DOF valve cell (§13.2) to a 2-DOF mucosal-wave lip pair (shearing \+ swinging modes) and adds an acoustic mass term ${L}_{r}\frac{dq}{dt}$ in the reed channel quadratic solver to stabilize the quasi-periodic A3 sax bifurcation noted in DECISIONS\_7 \#33.

## **2\. New Symplectic Geometry Synth Ideas for Suzu (§13 Expansion)**

Every proposed synth voice below adheres strictly to the **§13.1 DSP Class Table** ($\det J=1$ core, declared conformal damping, red/green negative test controls, and direct compatibility with the trace/ orbit bridge):

### **2.1 The Toda Lattice Soliton Ring (VOXO\_SUZU\_TODA)**

**The Physics:** While the §13.2 Verlet chain uses linear springs (which disperse) and the Duffing oscillator uses a local cubic (which bifurcates into chaos), the **Toda lattice** connects $N$ nodes on a closed ring via an exponential interaction potential:

$$H(q,p)=\sum\limits_{n=1}^{N}\frac{{p}_{n}^{2}}{2}+\frac{a}{b}\sum\limits_{n=1}^{N}\left({{e}^{-b({q}_{n+1}-{q}_{n})}+b({q}_{n+1}-{q}_{n})-1}\right)$$

*   
* **Why It Fits Suzu:** The Toda lattice is **completely integrable** (it possesses $N$ conserved quantities via its Lax pair matrix). Under symplectic Verlet integration ($\det J=1$), an initial strike or MPE pressure feed never breaks into chaotic noise—instead, it decomposes into **cnoidal waves and acoustic solitons** that circulate around the ring without dispersion.  
* **Musical Character & MPE Mapping:**  
  * At low nonlinearity ($b\rightarrow 0$, low MPE Press), the ring behaves like a pure linear modal marimba/flute.  
  * As MPE Press or Slide (CC 74\) increases $b$, the waveform sharpens into self-focusing soliton pulse trains—morphing smoothly from a warm sine-like hollow body into a bright clavinet/harpsichord pulse without aliasing spikes or amplitude runaway.  
  * MPE Swirl (0xA0) injects a topological chirality (net momentum flux around the ring), creating Barber-pole phaser-like spectral rotation.

### **2.2 Boris-Leapfrog Magnetic Phase-Space Gyroscope (VOXO\_SUZU\_LORENTZ)**

**The Physics:** A charged particle moving on a 2D potential surface $({x}_{1},{x}_{2})$ with momenta $({p}_{1},{p}_{2})$ under a perpendicular magnetic field $B({x}_{1},{x}_{2})$. This corresponds to a **non-canonical symplectic 2-form**:

$$\omega =d{p}_{1}\wedge d{x}_{1}+d{p}_{2}\wedge d{x}_{2}+B({x}_{1},{x}_{2})d{x}_{1}\wedge d{x}_{2}$$

*   
* **The Symplectic Integrator:** The Lorentz force ${F}_{B}=p\times B$ is perpendicular to velocity and does **zero mechanical work** ($\Delta H\equiv 0$). Discretized with the **Boris leapfrog algorithm** (half electric/potential kick $\rightarrow$ exact Cayley rotation in the magnetic field by angle ${\theta }_{B}=2\arctan (B\Delta t/2)$ $\rightarrow$ half potential kick), phase-space volume is conserved ($\det J=1$ exactly) and energy stays constant to machine precision at any magnetic field strength.  
* **Musical Character & Orbit Trace:**  
  * **Zeeman Harmonic Splitting:** The magnetic field splits every degenerate 2D oscillator mode ${\omega }_{0}$ into a chiral doublet ${\omega }_{\pm }=\sqrt{{\omega }_{0}^{2}+({\omega }_{B}/2{)}^{2}}\pm {\omega }_{B}/2$.  
  * **Native Binaural Stereo:** Outputting $({x}_{1},{x}_{2})$ directly to $(L,R)$ solves the carried §13.2 quadrature stereo item: at $B=0$ the voice is a mono planar string; as MPE Swirl (0xA0) increases $B$, the orbit precesses into a lush, phase-locked stereo rosette (hypocycloid/epitrochoid bell shimmer).  
  * **Visual Payoff:** Feeding $({x}_{1},{x}_{2})$ into trace/ (§13.2) draws intricate geometric spirographs onto the Sumi/Anod canvas in real time.

### **2.3 Symplectic Billiard / Bunimovich Stadium Oscillator (VOXO\_SUZU\_BILLIARD)**

**The Physics:** A piecewise-isometric Hamiltonian system. Inside a convex 2D phase-space boundary $\partial \Omega$, the state $(x,y)$ evolves via the exact §13.2 magic circle rotation ($\det J=1$). When the trajectory intersects $\partial \Omega$ with unit normal $\hat{n}$, it undergoes an exact specular reflection:

$$v'=v-2(v\cdot \hat{n})\hat{n}(\det J=-1,area-andnorm-preserving)$$

*   
* **Musical Character:**  
  * **Integrable Ellipse $\rightarrow$ Chaotic Stadium:** The boundary $\partial \Omega$ is a capsule/stadium parameterized by aspect ratio \$\\eccentricity\$ (CC 74\) and straight-wall length $L$ (Mod Wheel / CC 1).  
  * At $L=0$ (circle/ellipse), the billiard is integrable: orbits stay confined to smooth caustics, producing warm, woody, West-Coast wavefolded harmonics.  
  * As $L>0$ grows (**Bunimovich stadium**), the phase space transitions smoothly into ergodic wave-chaos, roughening the timbre into gritty metallic textures while the amplitude remains strictly confined to the stadium boundary by geometry.

### **2.4 Port-Hamiltonian Dirac Network (VOXO\_SUZU\_DIRAC)**

**The Physics (Completing §13.7's Port-Hamiltonian Program):** Generalizes the §13.2 hybrid string junction into a composable $K$\-port **Dirac structure**:

$$\dot{x}=(J-R)\nabla H(x),J=-{J}^{T}$$

*   
  Stepped via the **Cayley / implicit midpoint transform** $M=(I-\frac{\Delta t}{2}(J-R){)}^{-1}(I+\frac{\Delta t}{2}(J-R))$. Because $J$ is skew-symmetric, $M$ is strictly orthogonal ($\det M=1$, lossless energy routing) whenever $R=0$, and strictly passive for $R\succeq 0$.  
* **Musical Character:** Enables a **"Prepared Acoustic Network"** voice where a user patch routes energy between a reed/bow exciter, a 1D string, a non-linear rattle/preparation stop (unilateral collision like the §13.2 reed lay), and a modal bridge or 2D plate with zero risk of feedback runaway.

## **3\. Voxo (§12 Expansion): Decent Sampler Feature Completeness**

In v2.0, Voxo's v1 parser handles core zones, ADSR, loops, 1 low-pass SVF, basic bindings, and bus reverb/delay, while logging unsupported features in voxo/COMPAT\_REPORT.md (§12.3). For v3.0, we upgrade Voxo to **v2.0** by implementing the high-impact Decent Sampler features that real Pianobook libraries rely on—starting with **sustain mechanics** and **chorus**:

### **3.1 Advanced Sustain & Pedal Mechanics**

Currently, CC 64 simply holds the ADSR in its sustain stage until pedal-up (§12.2). Real Decent Sampler instruments require four distinct sustain mechanisms:

> 1. **Pedal Noise & Release Sample Triggers (trigger="cc", onLoCC/onHiCC):** Piano, Rhodes, and dulcimer presets trigger dedicated mechanical thump/damper-lift samples when CC 64 crosses 64 upward or downward. Voxo's normalizer drain will evaluate \<group trigger="cc"\> rules on CC 64 transitions.  
> 2. **Half-Pedaling / Continuous CC 64 Release Scaling:** Instead of a binary \< 64 / \>= 64 gate, when a note is released while $0<CC64<127$, the voice enters a **damped-sustain release curve** whose decay rate interpolates continuously between the preset's release time (pedal up) and a long sympathetic decay sustainDecay (pedal down).  
> 3. **Re-Pedaling (Catching a Decaying Tail):** Pressing CC 64 *after* Note Off while the release tail is still audible catches the voice at its current envelope level and freezes/slows its decay rate, rather than letting it die at the unpedaled release rate.  
> 4. **Choke Groups & Sustain-Gated Legato (tags, silencedByTags, silencingMode):** V1 parses tags (§12.3); V2 wires silencedByTags (normal \= trigger release, fast \= 5 ms linear fade) and group-level seqMode gating by CC 64 so presets with separate "pedal-up" and "pedal-down" resonant sample groups crossfade or switch seamlessly.

### **3.2 Chorus, Phaser & Modulation FX (\<effects\>)**

* **\<effect type="chorus"\> (Bus & Group Level):** A real-time-safe stereo quadrature LFO delay line (pre-allocated inside VoxoInstrument at load time, zero audio-thread allocation per §12.1) supporting DS's exact parameters: mix, modDepth, modRate, and stereo phase offset ($9{0}^{\circ }$), with 4-point Hermite read pointers (reusing the §12.2 pitch interpolator).  
* **\<effect type="phaser"\>:** A 4-stage or 6-stage cascaded all-pass filter bank swept by a stereo LFO (mix, modDepth, modRate, feedback, centerFrequency).  
* **CC Map Extension (presets/SCHEMA.md):** Extend the bus CC targets (currently 1000–1005 for reverb/delay, §12.3) with VOXO\_CTL\_CHORUS\_MIX (1006), VOXO\_CTL\_CHORUS\_RATE (1007), and VOXO\_CTL\_CHORUS\_DEPTH (1008).

### **3.3 Other Common Decent Sampler Parameters to Add**

* **Expanded Filter Types & Filter Envelopes:**  
  * Upgrade the §12.2 per-voice 2-pole SVF from low-pass-only to support DS's full filter family: lowpass, highpass, bandpass, notch, peak, and lowpass\_1pl (1-pole). Since a Chamberlin/Zavalishin SVF already computes low-pass, high-pass, and band-pass outputs simultaneously in the same loop, exposing highpass, bandpass, and notch costs **zero extra multiplies per sample**.  
  * Wire \<modulators\>\<envelope\> to filter frequency and resonance so pluck/synth presets with filter sweeps play accurately.  
* **Bus 3-Band EQ, Gain & Waveshaper (\<effect type="eq"\>, \<effect type="gain"\>, \<effect type="wave\_shaper"\>):**  
  * Biquad low-shelf, peaking mid, and high-shelf on the bus, plus wave\_shaper (drive, boost, outputLevel) for warm tape/tube saturation on vintage keys presets.  
* **LFOs & Full \<modulators\> Matrix:**  
  * Implement \<lfo\> modulators (frequency, waveform="sine|triangle|square|saw", offset, phase, sync="free|tempo" using the host-supplied bpm from §3.4).  
  * Support multi-target \<binding\> modulation for group/voice amp, pan, pitch (fine tuning / vibrato), filter\_cutoff, and effect parameters.  
* **Legato / Monophonic Mode & Glide (legato, glideTime, glideMode):**  
  * Many solo string/woodwind DS presets use monophonic group modes with portamento (glideTime). Wiring this complements MPE pitch bend for classic single-channel keyboards (§2.4).  
* **Compat Report Pruning (voxo/COMPAT\_REPORT.md):**  
  * Update the canonical sentence table (§12.3) so chorus, eq and other filters, and modulators no longer emit unsupported warnings, leaving only genuinely out-of-scope items (convolution, note sequences, streaming, custom skin UI).

## **4\. Proposed v3.0 Roadmap (\_work/ROADMAP\_6.md)**

This roadmap follows the exact step structure and verification gates of Parts 1–9:

| Phase | Step | Title | Core Deliverables & Acceptance Gates (DONE Criteria) |
| :---- | :---- | :---- | :---- |
| **Phase 11: Suzu Foundation & §13.7 Deferred Physics** | **71** | **SoA SIMD Engine & ×4 Half-Band Oversampler** | Refactor VerletChain and ModalLattice into Structure-of-Arrays SIMD (AVX2 / NEON / WASM SIMD128); add optional ×4 polyphase half-band FIR oversampler for fold shear (DECISIONS\_7 \#9). **Gate:** 16×80-node chains drop below 6% callback CPU; bit-tolerance check against scalar reference. |
|  | **72** | **2D Symplectic FDTD Membranes, Plates & Gongs** | Implement VOXO\_SUZU\_MEMBRANE (§13.7): per-voice 16×16 SIMD leapfrog grid \+ shared 64×64 bus resonator struck at sumi\_layout\_probe $(x,y)$ coordinates; Berger symplectic momentum shear for gong crash. **Gates:** 10-min undamped energy drift $<0.005dB$; CFL gate rejects ${\lambda }_{2D}>2$; red control ($1.05\times$ CFL) blows up within 100 steps; sound profile generated. |
|  | **73** | **Physical Stick-Slip (Helmholtz) Bow & Strike Profiles** | Implement implicit hyperbolic stick-slip bow (bow\_mode \= 1\) with power-limited port ledger; wire layout cell strike-position profile (DECISIONS\_7 \#12) and sustained press drive. **Gates:** Helmholtz corner sharpening verified via spectrogram; energy held $\leq \int\limits_{}^{}{F}_{bow}{v}_{bow}dt$. |
|  | **74** | **Acoustic Bore v2: Toneholes, Two-Mass Lips & Brass Shock** | Add shunt tonehole branch ports driven by CC 110–112 \+ register vent; upgrade valve to 2-DOF lips \+ reed channel flow inertia (DECISIONS\_7 \#23, fixing the quasi-periodic A3 sax split of \#33); add $(\gamma +1)/2$ bore shock shear. **Gates:** Sax A3 sidebands suppressed below $-45dB$; mouth-power ledger $k[9]\leq k[8]$ holds under shock steepening. |
|  | **75** | **Editable Modal Tables, Scala (.scl) & Unison Splitting** | Expose user-editable modal ratio/decay arrays in voxo\_suzu\_params\_t and presets/SCHEMA.md; add .scl/.kbm microtonal table loader for both Suzu and Voxo (§13.7, §14.6); add a unison mode-splitting patch (DECISIONS\_7 \#34). |
| **Phase 12: New Symplectic Synth Voices** | **76** | **Toda Lattice Soliton Ring (VOXO\_SUZU\_TODA)** | Implement $N$\-node exponential-spring Toda ring under symplectic Verlet; map Press/CC 74 to nonlinearity $b$ and Swirl (0xA0) to ring momentum flux. **Gates:** 10-min lossless energy drift $<0.001dB$; soliton pulse compression verified without aliasing blow-up; sound profile. |
|  | **77** | **Boris-Leapfrog Magnetic Gyroscope (VOXO\_SUZU\_LORENTZ)** | Implement 4D phase-space particle in magnetic field $B({x}_{1},{x}_{2})$ via Boris integrator ($\det J=1$); map $({x}_{1},{x}_{2})$ to stereo $(L,R)$ and trace/ orbit bridge. **Gates:** Exact energy conservation across $B\in [0,50]$; Zeeman doublet splitting matches $\pm {\omega }_{B}/2$ within $0.1cent$; spirograph trace verified in Anod. |
|  | **78** | **Port-Hamiltonian Dirac Network & Billiard Cell** | Implement Cayley midpoint Dirac structure coupling string \+ rattle stop \+ plate (VOXO\_SUZU\_DIRAC, §13.7) and Bunimovich stadium billiard oscillator (VOXO\_SUZU\_BILLIARD). **Gates:** Passivity probe $\leq 1.000$ across arbitrary skew-symmetric $J$; KAM-to-ergodic spectrum sweep charted. |
|  | **79** | **Per-Voice Chaotic Modulator & 4-Slot Mod Matrix** | Promote the §13.2 projected double-pendulum modulator from patch-global to per-voice (DECISIONS\_7 \#16) and expand mod\_target to a 4-slot modulation matrix. |
| **Phase 13: Voxo 2.0 (Decent Sampler Expansion)** | **80** | **Sustain Pedal v2, Choke Groups & Pedal Noises** | Add continuous half-pedaling (sustainDecay), re-pedaling catch, \<group trigger="cc"\> pedal noise groups, and silencedByTags (normal/fast choke) (§12.2–§12.3). **Gates:** Headless bounce test of pedal-down, half-pedal, re-pedal, and hi-hat choke groups; zero audio-thread allocations. |
|  | **81** | **Chorus & Phaser Effects \+ CC Map Targets** | Implement real-time-safe \<effect type="chorus"\> and \<effect type="phaser"\> with pre-allocated Hermite delay lines; add CC map targets 1006–1008 in presets/SCHEMA.md (§12.3). |
|  | **82** | **Multi-Mode Filters, Filter Envelopes & Bus EQ/Shaper** | Expose highpass, bandpass, notch, peak, lowpass\_1pl on the per-voice SVF; add \<effect type="eq"\> (3-band) and \<effect type="wave\_shaper"\> (§12.3). |
|  | **83** | **Full \<modulators\> LFO/Envelope Matrix & Compat Update** | Wire multi-target \<lfo\> (free & BPM-synced) and \<envelope\> modulators; add group legato/glideTime; update voxo/COMPAT\_REPORT.md test goldens and run a 25M-iteration voxo\_fuzz pass (§12.3). |
|  | **84** | **Preset Instrument Slot, Replay WAV Bounce & Audio Picker** | Close §16 Sound carry-overs: reference DS library by name/hash in presets/, productize \--replay-wav offline bounce in all shells, and add desktop audio output device selection (DECISIONS\_6 \#42). |
| **Phase 14: Visual & Layout Carry-Overs** | **85** | **Anod Deterministic Seed, Strain Buffer & Chirikov Grid** | Fix Anod's gesture-ABI run-to-run reproducibility (DECISIONS\_7 \#25), evaluate the long-exposure strain buffer (§10.4), and tune the Chirikov feel grid (DECISIONS\_5 \#89). |
|  | **86** | **Woodwind Boehm Layout, Split Surface & Strip Preset-Next** | Add Boehm key-system layout (SUMI\_LAYOUT\_BOEHM), two-handed split screen layouts (§14.6), and the strip's Preset-Next button (§15.2). |
| **Phase 15: Web Lab, Docs & v3.0 Release** | **87** | **Suzu Web Lab v2, Docs & v3.0 Release Spine** | Add interactive 2D Membrane, Toda Soliton, and Boris Gyroscope panels to web/suzu/ (tools/suzu\_lab\_gate.mjs); update the Suzu & Voxo documentation books; run the 5-platform release pipeline for 3.0.0. |

