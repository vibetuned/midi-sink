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
// api: { drop, tine, vortex, wake, pinch, midi, param, setParam, aspect }

const rings = (api, cx = 0.5, cy = 0.5, r = 0.14, n = 8) => {
  for (let i = 0; i < n; i++) api.drop(cx, cy, r, 0);
};

export const SCENES = {
  drop: {
    title: 'Drop expansion',
    formula: 'P_src = C + (P − C)·√(1 − r²/‖P − C‖²)   for ‖P − C‖ ≥ r',
    params: [
      { key: 'r', sym: 'r', label: 'drop radius', min: 0.02, max: 0.3, step: 0.005, def: 0.14 },
      { key: 'n', sym: 'n', label: 'drops', min: 1, max: 16, step: 1, def: 8 },
    ],
    setup(api, v) { rings(api, 0.5, 0.5, v.r, v.n); },
  },
  tine: {
    title: 'Tine / comb stroke',
    formula: 'P_src = P − z·D̂·α/(α + d)',
    params: [
      { key: 'alpha', sym: 'α', label: 'sharpness', min: 0.005, max: 0.2, step: 0.005, def: 0.05 },
      { key: 'z', sym: 'z', label: 'magnitude', min: 0.01, max: 0.4, step: 0.01, def: 0.1 },
    ],
    setup(api, v) { rings(api); api.tine(0.2, 0.3, 0.8, 0.7, v.alpha, v.z); },
  },
  vortex: {
    title: 'Vortex — exponential (Jaffer)',
    formula: 'θ(r) = A·exp(−r/R)   rotate P by −θ',
    params: [
      { key: 'A', sym: 'A', label: 'angle', min: -6.28, max: 6.28, step: 0.05, def: 2.0 },
      { key: 'R', sym: 'R', label: 'radius', min: 0.05, max: 0.5, step: 0.01, def: 0.25 },
    ],
    setup(api, v) { rings(api); api.vortex(0.6, 0.4, v.A, v.R, 0); },
  },
  rankine: {
    title: 'Vortex — Rankine (rigid core)',
    formula: 'θ(r) = ω          for r < R\nθ(r) = ω·R²/r²    for r ≥ R',
    params: [
      { key: 'omega', sym: 'ω', label: 'core angle', min: -6.28, max: 6.28, step: 0.05, def: 2.0 },
      { key: 'R', sym: 'R', label: 'core radius', min: 0.05, max: 0.5, step: 0.01, def: 0.25 },
    ],
    setup(api, v) { rings(api); api.vortex(0.5, 0.5, v.omega, v.R, 1); },
  },
  wake: {
    title: 'Dipolar wake (stylus)',
    formula: 'x_src = x − d·a²(x² − y²)/r⁴,  y_src = y − d·2a²xy/r⁴   (r > a)\nP_src = P − d   (rigid shift) inside the tip (r ≤ a)',
    params: [
      { key: 'a', sym: 'a', label: 'tip radius', min: 0.005, max: 0.08, step: 0.001, def: 0.03 },
      { key: 'len', sym: 'd', label: 'stroke length', min: 0.02, max: 0.5, step: 0.01, def: 0.25 },
    ],
    setup(api, v) {
      rings(api);
      const steps = 24, x0 = 0.5 - v.len / 2;
      for (let i = 0; i < steps; i++) {
        const t0 = i / steps, t1 = (i + 1) / steps;
        api.wake(x0 + v.len * t0, 0.5, x0 + v.len * t1, 0.5, v.a);
      }
    },
  },
  pinch: {
    title: 'Hamiltonian pinch',
    formula: 'x_src = x·e^{+k·w(s)},  y_src = y·e^{−k·w(s)},  s = xy,  w(s) = e^{−|s|/S}',
    params: [
      { key: 'k', sym: 'k', label: 'strength per pass', min: -0.1, max: 0.1, step: 0.002, def: 0.02 },
      { key: 'theta', sym: 'θ', label: 'fold axis (rad)', min: 0, max: 3.14, step: 0.02, def: 0.6 },
      { key: 'passes', sym: 'n', label: 'passes', min: 1, max: 80, step: 1, def: 40 },
      { key: 'variant', sym: 'v', label: '0 saddle · 1 crossed tines', min: 0, max: 1, step: 1, def: 0 },
    ],
    setup(api, v) {
      api.setParam('pinch_variant', v.variant);
      rings(api);
      for (let i = 0; i < v.passes; i++) api.pinch(0.5, 0.5, v.k, v.theta);
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
    ],
    setup(api, v) {
      api.setParam('ripple_angle', v.angle * Math.PI / 180);
      api.setParam('ripple_bake', v.bake);
      api.mapCC(102, 7); api.mapCC(103, 8);          // the ripple dims ship unmapped
      rings(api);
      api.midi(0xB0, 103, v.k);
      api.midi(0xB0, 102, v.A);
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
    formula: 'θ(r) = Γ·Δt/(2πr²)·(1 − exp(−r²/r_c²)),   r_c = the voice\'s boundary',
    params: [
      { key: 'gamma', sym: 'Γ', label: 'amount (0xA0 pressure)', min: 0, max: 127, step: 1, def: 100 },
      { key: 'vel', sym: 'v', label: 'strike velocity (→ r_c)', min: 20, max: 127, step: 1, def: 100 },
      { key: 'frames', sym: 't', label: 'stir frames', min: 10, max: 400, step: 10, def: 120 },
    ],
    async setup(api, v) {
      api.setParam('layout', 1);                     // chromatic grid: F#4 sits near center
      api.midi(0xB0, 101, 0); api.midi(0xB0, 100, 6); api.midi(0xB0, 6, 15);   // MCM → MPE
      api.midi(0x91, 66, v.vel);
      await api.frames(2);
      // the voice's own rings, then stir them with poly pressure on its note
      for (let i = 0; i < 6; i++) api.drop(0.535, 0.5, 0.10, 0);
      await api.frames(2);
      for (let i = 0; i < v.frames; i++) { api.midi(0xA1, 66, v.gamma); await api.frames(1); }
      api.midi(0xA1, 66, 0);
      api.midi(0x81, 66, 64);
    },
  },
  scroll: {
    title: 'Piano-roll scroll (field motion)',
    formula: 'P_src = P − v̂·s·dt,   s = (bpm/60)·roll_speed  canvas lengths/s',
    params: [
      { key: 'bpm', sym: 'bpm', label: 'tempo', min: 20, max: 300, step: 1, def: 120 },
      { key: 'roll', sym: 'ρ', label: 'roll speed (canvas/beat)', min: 0.01, max: 0.25, step: 0.005, def: 0.0625 },
    ],
    setup(api, v) {
      api.setParam('layout', 3);
      api.setParam('bpm', v.bpm);
      api.setParam('roll_speed', v.roll);
      for (let i = 0; i < 6; i++) api.drop(0.12, 0.3 + 0.08 * i, 0.05, 0);
    },
    live(api, v) { api.setParam('bpm', v.bpm); api.setParam('roll_speed', v.roll); },
  },
};

export function sceneNames() { return Object.keys(SCENES); }
