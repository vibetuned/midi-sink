// scenes.js — the scene/embed API's operator scenes (Phase 5 §5, §6).
// One scene per deformation operator; each exposes the FORMULA'S SYMBOLS as
// sliders and a deterministic setup, so the docs' live examples are this
// artifact — the release wasm — and nothing else. Positions are normalized
// canvas coordinates (the C-ABI's), radii in canvas-height units.
//
// A scene: { title, formula, params: [{key, sym, label, min, max, step, def}],
//            setup(api, v), live?(api, v, prev) }
//   setup: fresh sheet, then the deterministic script for the values v
//   live:  optional — apply a slider change without replaying (defaults to
//          replaying the scene)
// api: { drop, tine, vortex, wake, pinch, midi, mapCC, param, setParam,
//        probe(x, y) -> {note, cx, cy, r} | null, frames(n), aspect }
//
// Two conventions, so the mathematics is SEEN, not just its result (Step 26):
//  * every scene is PACED — `⏱` = frames between steps (0 = instant) — and the
//    operators that compose exactly (tine, vortex, pinch, wake sub-steps) are
//    applied as a run of small passes, which also demonstrates the composition
//    invariant (40 passes of z/40 = 1 pass of z);
//  * every scene works on TWO ring clusters, A (top-left) and B (bottom-right),
//    so one operator can show two orientations or two signs at once —
//    horizontal vs vertical tine, counter-rotating vortices, perpendicular fold
//    axes, drop vs pressure feed. (The swirl is the exception: one voice at the
//    centre stirring four corner pools — its far field is the picture.)

const A = { x: 0.30, y: 0.30 };
const B = { x: 0.70, y: 0.70 };
// Rotations (vortex, Rankine) are centred a little OFF the cluster — a rotation
// of concentric circles about their own centre is invisible by design.
const OFF = { x: 0.07, y: 0.05 };
const PACE = { key: 'pace', sym: '⏱', label: 'frames per step (0 = instant)', min: 0, max: 10, step: 1, def: 2 };
const wait = (api, v, n = 1) => (v.pace > 0 ? api.frames(v.pace * n) : Promise.resolve());

const rings = async (api, v, cx, cy, r = 0.10, n = 6) => {
  for (let i = 0; i < n; i++) { api.drop(cx, cy, r, 0); await wait(api, v); }
};
// Band parity alternates with the GLOBAL drop counter, so two sites must be fed
// in the order A B | B A | A B | B A …: each site then sees alternating parity
// (rings). A plain A B A B … would give A one parity and B the other — a solid
// disk and a hollow one.
const twoClusters = async (api, v, r = 0.10, n = 6) => {
  for (let i = 0; i < n; i++) {
    const first = i % 2 === 0 ? A : B, second = first === A ? B : A;
    api.drop(first.x, first.y, r, 0); api.drop(second.x, second.y, r, 0);
    await wait(api, v);
  }
};

// MPE session for the voice-driven scenes (swirl, feed): chromatic grid — the one
// layout whose cells cover the sheet in reading order, so the probe can hand us
// the note that lives at A and at B. Falls back to two mid-grid notes.
const mpe = (api) => {
  api.setParam('layout', 1);
  api.midi(0xB0, 101, 0); api.midi(0xB0, 100, 6); api.midi(0xB0, 6, 15);   // MCM → MPE, 15 members
};
const voiceAt = (api, p, fallbackNote) => {
  const c = api.probe ? api.probe(p.x, p.y) : null;
  return c ? { note: c.note, x: c.cx, y: c.cy } : { note: fallbackNote, x: p.x, y: p.y };
};

export const SCENES = {
  drop: {
    title: 'Drop expansion',
    formula: 'P_src = C + (P − C)·√(1 − r²/‖P − C‖²)   for ‖P − C‖ ≥ r',
    params: [
      { key: 'r', sym: 'r', label: 'drop radius', min: 0.02, max: 0.3, step: 0.005, def: 0.10 },
      { key: 'n', sym: 'n', label: 'drops', min: 1, max: 16, step: 1, def: 6 },
      PACE,
    ],
    async setup(api, v) { await twoClusters(api, v, v.r, v.n); },
  },
  feed: {
    title: 'Ink feed (pressure) vs. drops',
    formula: 'r_pass = √((R + ΔR)² − R²),   ΔR ∝ pressure·dt   (A: separate drops · B: one fed drop)',
    params: [
      { key: 'vel', sym: 'v', label: 'strike velocity (→ r)', min: 20, max: 127, step: 1, def: 90 },
      { key: 'press', sym: 'p', label: 'pressure (0xD0)', min: 1, max: 127, step: 1, def: 90 },
      { key: 'frames', sym: 't', label: 'feed frames', min: 10, max: 400, step: 10, def: 150 },
      PACE,
    ],
    async setup(api, v) {
      mpe(api);
      const a = voiceAt(api, A, 52), b = voiceAt(api, B, 81);
      // B: a strike, then PRESSURE held — one band whose boundary keeps growing.
      // A: a strike, then the same-size drop dropped again and again — rings.
      // B strikes FIRST: the even drop counter is the inked band, so the fed
      // drop reads as a dark disk growing rather than a clear one.
      api.midi(0x92, b.note, v.vel);
      api.midi(0x91, a.note, v.vel);
      await api.frames(2);
      const perRing = Math.max(1, Math.round(v.frames / 5));
      for (let i = 0; i < v.frames; i++) {
        api.midi(0xD2, v.press, 0);
        if (i % perRing === perRing - 1) api.midi(0x91, a.note, v.vel);   // re-strike = a new drop (same channel steals the voice)
        await api.frames(1);
      }
      api.midi(0xD2, 0, 0);
      api.midi(0x81, a.note, 64); api.midi(0x82, b.note, 64);
    },
  },
  tine: {
    title: 'Tine / comb stroke',
    formula: 'P_src = P − z·D̂·α/(α + d)      (composes exactly: n passes of z/n = one pass of z)',
    params: [
      { key: 'alpha', sym: 'α', label: 'sharpness', min: 0.005, max: 0.2, step: 0.005, def: 0.05 },
      { key: 'z', sym: 'z', label: 'magnitude', min: 0.01, max: 0.4, step: 0.01, def: 0.12 },
      { key: 'n', sym: 'n', label: 'passes', min: 1, max: 60, step: 1, def: 20 },
      PACE,
    ],
    async setup(api, v) {
      await twoClusters(api, v);
      for (let i = 0; i < v.n; i++) {
        api.tine(0.05, A.y, 0.55, A.y, v.alpha, v.z / v.n);          // horizontal through A
        api.tine(B.x, 0.45, B.x, 0.95, v.alpha, v.z / v.n);          // vertical through B
        await wait(api, v);
      }
    },
  },
  vortex: {
    title: 'Vortex — exponential (Jaffer)',
    formula: 'θ(r) = A·exp(−r/R)   rotate P by −θ      (A at top-left, −A at bottom-right, each just off its cluster)',
    params: [
      { key: 'A', sym: 'A', label: 'angle', min: -12.6, max: 12.6, step: 0.1, def: 6.0 },
      { key: 'R', sym: 'R', label: 'radius', min: 0.05, max: 0.5, step: 0.01, def: 0.22 },
      { key: 'n', sym: 'n', label: 'passes', min: 1, max: 90, step: 1, def: 48 },
      PACE,
    ],
    async setup(api, v) {
      await twoClusters(api, v);
      for (let i = 0; i < v.n; i++) {
        api.vortex(A.x + OFF.x, A.y + OFF.y, v.A / v.n, v.R, 0);
        api.vortex(B.x - OFF.x, B.y - OFF.y, -v.A / v.n, v.R, 0);
        await wait(api, v);
      }
    },
  },
  rankine: {
    title: 'Vortex — Rankine (rigid core)',
    formula: 'θ(r) = ω          for r < R\nθ(r) = ω·R²/r²    for r ≥ R      (ω at top-left, −ω at bottom-right, each just off its cluster)',
    params: [
      { key: 'omega', sym: 'ω', label: 'core angle', min: -12.6, max: 12.6, step: 0.1, def: 5.0 },
      { key: 'R', sym: 'R', label: 'core radius', min: 0.05, max: 0.5, step: 0.01, def: 0.16 },
      { key: 'n', sym: 'n', label: 'passes', min: 1, max: 90, step: 1, def: 48 },
      PACE,
    ],
    async setup(api, v) {
      await twoClusters(api, v);
      for (let i = 0; i < v.n; i++) {
        api.vortex(A.x + OFF.x, A.y + OFF.y, v.omega / v.n, v.R, 1);
        api.vortex(B.x - OFF.x, B.y - OFF.y, -v.omega / v.n, v.R, 1);
        await wait(api, v);
      }
    },
  },
  wake: {
    title: 'Dipolar wake (stylus)',
    formula: 'x_src = x − d·a²(x² − y²)/r⁴,  y_src = y − d·2a²xy/r⁴   (r > a)\nP_src = P − d   (rigid shift) inside the tip (r ≤ a)',
    params: [
      { key: 'a', sym: 'a', label: 'tip radius', min: 0.005, max: 0.08, step: 0.001, def: 0.025 },
      { key: 'len', sym: 'd', label: 'stroke length', min: 0.02, max: 0.5, step: 0.01, def: 0.3 },
      PACE,
    ],
    async setup(api, v) {
      await twoClusters(api, v);
      const steps = 30;
      for (let i = 0; i < steps; i++) {
        const t0 = i / steps, t1 = (i + 1) / steps;
        api.wake(A.x - v.len / 2 + v.len * t0, A.y, A.x - v.len / 2 + v.len * t1, A.y, v.a);   // left → right through A
        api.wake(B.x, B.y - v.len / 2 + v.len * t0, B.x, B.y - v.len / 2 + v.len * t1, v.a);   // top → bottom through B
        await wait(api, v);
      }
    },
  },
  viscous: {
    title: 'Viscous stroke (2-D Stokeslet)',
    formula: 'd_x = d/(2L)[Φ(S₁)−Φ(S₀)] − (d/L)(y²/r²)[χ(S₁)−χ(S₀)],  d_y = (d/L)(xy/r²)[χ(S₁)−χ(S₀)]\nχ = (1−e^−S)/S, Φ = χ + E₁,  S₀ = r²/a², S₁ = r²/l²,  L = ln(l/a)',
    params: [
      { key: 'a', sym: 'a', label: 'tip radius', min: 0.005, max: 0.08, step: 0.001, def: 0.025 },
      { key: 'l', sym: 'l', label: 'spread l/a (diffusion length in tip radii)', min: 1.5, max: 12, step: 0.1, def: 3 },
      { key: 'len', sym: 'd', label: 'stroke length', min: 0.02, max: 0.5, step: 0.01, def: 0.3 },
      PACE,
    ],
    async setup(api, v) {
      api.setParam('wake_profile', 1);
      api.setParam('wake_spread', v.l);
      await twoClusters(api, v);
      const steps = 30;
      for (let i = 0; i < steps; i++) {
        const t0 = i / steps, t1 = (i + 1) / steps;
        api.wake(A.x - v.len / 2 + v.len * t0, A.y, A.x - v.len / 2 + v.len * t1, A.y, v.a);   // left → right through A
        api.wake(B.x, B.y - v.len / 2 + v.len * t0, B.x, B.y - v.len / 2 + v.len * t1, v.a);   // top → bottom through B
        await wait(api, v);
      }
      api.setParam('wake_profile', 0);
    },
  },
  pinch: {
    title: 'Hamiltonian pinch',
    formula: 'x_src = x·e^{+k·w(s)},  y_src = y·e^{−k·w(s)},  s = xy,  w(s) = e^{−|s|/S}      (fold θ at top-left, θ + 90° at bottom-right)',
    params: [
      { key: 'k', sym: 'k', label: 'strength per pass', min: -0.1, max: 0.1, step: 0.002, def: 0.008 },
      { key: 'theta', sym: 'θ', label: 'fold axis (rad)', min: 0, max: 3.14, step: 0.02, def: 0.6 },
      { key: 'passes', sym: 'n', label: 'passes', min: 1, max: 80, step: 1, def: 30 },
      { key: 'variant', sym: 'v', label: '0 saddle · 1 crossed tines', min: 0, max: 1, step: 1, def: 0 },
      PACE,
    ],
    async setup(api, v) {
      api.setParam('pinch_variant', v.variant);
      await twoClusters(api, v);
      for (let i = 0; i < v.passes; i++) {
        api.pinch(A.x, A.y, v.k, v.theta);
        api.pinch(B.x, B.y, v.k, v.theta + Math.PI / 2);
        await wait(api, v);
      }
    },
  },
  ripple: {
    title: 'Sine ripple (waved comb)',
    formula: 'x_src = x − A·sin(k·y + φ),  y_src = y   (pure shear, det = 1)',
    params: [
      { key: 'A', sym: 'A', label: 'amount (CC 102)', min: 0, max: 127, step: 1, def: 96 },
      { key: 'k', sym: 'k', label: 'wavelength (CC 103)', min: 0, max: 127, step: 1, def: 32 },
      { key: 'angle', sym: 'φ', label: 'frame angle (deg)', min: 0, max: 180, step: 1, def: 0 },
      { key: 'bake', sym: 'b', label: '0 live · 1 bake', min: 0, max: 1, step: 1, def: 0 },
      PACE,
    ],
    async setup(api, v) {
      api.setParam('ripple_angle', v.angle * Math.PI / 180);
      api.setParam('ripple_bake', v.bake);
      api.mapCC(102, 7); api.mapCC(103, 8);          // the ripple dims ship unmapped
      await twoClusters(api, v);
      api.midi(0xB0, 103, v.k);
      const steps = 24;                              // the amount ramps in, so the shear is seen growing
      for (let i = 1; i <= steps; i++) { api.midi(0xB0, 102, Math.round(v.A * i / steps)); await wait(api, v); }
    },
    live(api, v) {
      api.setParam('ripple_angle', v.angle * Math.PI / 180);
      api.setParam('ripple_bake', v.bake);
      api.midi(0xB0, 103, v.k);
      api.midi(0xB0, 102, v.A);
    },
  },
  lamb_oseen: {
    title: 'Lamb–Oseen swirl (per-note pressure)',
    formula: 'θ(r) = Γ·Δt/(2πr²)·(1 − exp(−r²/r_c²)),   r_c = the voice\'s boundary      (one voice at the centre; four pools in the far field)',
    params: [
      { key: 'gamma', sym: 'Γ', label: 'amount (0xA0 pressure)', min: 0, max: 127, step: 1, def: 127 },
      { key: 'vel', sym: 'v', label: 'strike velocity (→ r_c)', min: 1, max: 127, step: 1, def: 24 },
      { key: 'frames', sym: 't', label: 'stir frames', min: 10, max: 1200, step: 10, def: 900 },
      PACE,
    ],
    async setup(api, v) {
      mpe(api);
      // The swirl is per-note: the voice's strike IS the core (r_c). Far from
      // the core θ ≈ ΓΔt/(2πr²) regardless of r_c, so a SOFT strike leaves a
      // dot in the middle and costs the far field nothing — the picture is four
      // ring pools wound into spirals by a long stir.
      const c = voiceAt(api, { x: 0.5, y: 0.5 }, 66);
      api.midi(0x91, c.note, v.vel);
      await api.frames(2);
      api.drop(c.x, c.y, 0.045, 1);                  // a CLEAR drop over the strike: the core is water, nothing marks the centre
      await api.frames(1);
      const pools = [{ x: 0.34, y: 0.34 }, { x: 0.66, y: 0.34 }, { x: 0.66, y: 0.66 }, { x: 0.34, y: 0.66 }];
      for (let i = 0; i < 4; i++) {                  // forward, reverse, … — alternating parity per pool (see twoClusters)
        const order = i % 2 === 0 ? pools : [...pools].reverse();
        for (const p of order) api.drop(p.x, p.y, 0.055, 0);
        await wait(api, v);
      }
      await api.frames(2);
      for (let i = 0; i < v.frames; i++) { api.midi(0xA1, c.note, v.gamma); await api.frames(1); }
      api.midi(0xA1, c.note, 0);
      api.midi(0x81, c.note, 64);
    },
  },
  scroll: {
    title: 'Piano-roll scroll (field motion)',
    formula: 'P_src = P − v̂·s·dt,   s = (bpm/60)·roll_speed  canvas lengths/s',
    params: [
      { key: 'bpm', sym: 'bpm', label: 'tempo', min: 20, max: 300, step: 1, def: 120 },
      { key: 'roll', sym: 'ρ', label: 'roll speed (canvas/beat)', min: 0.01, max: 0.25, step: 0.005, def: 0.0625 },
      PACE,
    ],
    async setup(api, v) {
      api.setParam('layout', 3);
      api.setParam('bpm', v.bpm);
      api.setParam('roll_speed', v.roll);
      // Drops born on the now-line one after another: the earlier ones have already drifted.
      for (let i = 0; i < 8; i++) { api.drop(0.12, 0.25 + 0.07 * i, 0.045, 0); await wait(api, v, 8); }
    },
    live(api, v) { api.setParam('bpm', v.bpm); api.setParam('roll_speed', v.roll); },
  },
};

export function sceneNames() { return Object.keys(SCENES); }
