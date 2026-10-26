// membrane.js — the Suzu lab's membrane and gong (v3.0 step 92; the voice of step 79, DECISIONS_12 #3; TO_PROJECT_SPEC §13.9).
//
// Voice kind 9: a two-dimensional leapfrog inside a simply-supported zero ring, the grid cut per note under the
// Courant bar (1.95; the gong's own 1.80). The sheet is drawn node by node from the engine's displacement array; the
// gong's stretching (von Kármán: c² + αE) raises the pitch of a hard strike and lets it settle. ?gate=membrane checks
// the swell's landing and settling, the drum on the note, the leapfrog's invariant, and the red control — the CFL
// gate bypassed with λ₂D forced past the bar — caught by the lab.
import { QS, $, css, noteName, noteHz, clamp, Lab, bindPower, ensureStarted, buildKeys, markKeys, bindKeymap, drawSpectrum,
  Portrait, Strip, banner, footer, navigation, offlineRun, gateGuard, register, signed, peakScan, fit, DOCS_ROOT } from './lab-core.js';

const KIND = 9;
// the patch table's two (voxo/src/suzu_patches.cpp): the drum and the gong
const DRUM = { voice_kind: KIND, drum_size: 16, drum_kappa: 0.02, drum_alpha: 0, drum_cap: 1, decay_s: 2, decay_bright: 0.15, pluck: 0.3, pickup: 0.32,
  level: 0.3, attack_s: 0.002, release_s: 0.6, cutoff_hz: 20000, resonance: 0, drum_cfl_gate: 1, drum_cfl: 0 };
const GONG = { ...DRUM, drum_kappa: 0.08, drum_alpha: 0.8, drum_cap: 1.5, decay_s: 6, decay_bright: 0.05, pluck: 0.42, pickup: 0.3, attack_s: 0.004, release_s: 2 };
const lab = new Lab({ params: { ...DRUM }, traceMask: 1 << KIND, traceDecim: 4, recentPoints: 256, snapHz: 30, strobe: 1 });
const S = { ins: null, dots: [], serial: -1, h0: 0, hNow: 0, uScale: 1e-6, hz: 0 };
const dB = (r) => 10 * Math.log10(Math.max(r, 1e-30));

// ---- the browser check ----
async function gate() {
  const sr = 48000, note = 48, f0 = noteHz(note);
  const line = (x, t0, t1) => peakScan(x.subarray(Math.floor(t0 * sr), Math.floor(t1 * sr)), sr, f0 * 0.9, f0 * 1.25, 0.25).hz;
  const cents = (hz) => 1200 * Math.log2(hz / f0);
  const g1 = await offlineRun({ params: { ...GONG }, script: [[0, 0x91, note, 127]], seconds: 2.0, snapHz: 10 });
  const g2 = await offlineRun({ params: { ...GONG }, script: [[0, 0x91, note, 30]], seconds: 2.0, snapHz: 10 });
  const d1 = await offlineRun({ params: { ...DRUM }, script: [[0, 0x91, note, 127]], seconds: 2.0, snapHz: 10 });
  const gHardEarly = cents(line(g1.x, 0.03, 0.33)), gHardLate = cents(line(g1.x, 1.2, 1.9)), gSoftEarly = cents(line(g2.x, 0.03, 0.33)), dEarly = cents(line(d1.x, 0.03, 0.33));
  // the invariant: the drum lossless, the leapfrog's own energy (k[7]) from the inspection
  const inv = await offlineRun({ params: { ...DRUM, decay_s: 0, decay_bright: 0 }, script: [[0, 0x91, note, 100]], seconds: 3.0, snapHz: 20 });
  const hs = inv.snaps.filter((s) => s.inspect.length && s.block * 128 / sr > 0.3).map((s) => s.inspect[0].k[7]);
  const drift = hs.length > 4 ? Math.abs(dB(hs[hs.length - 1] / hs[0])) : Infinity;
  const lam = inv.snaps.find((s) => s.inspect.length)?.inspect[0].k[4] ?? 0;
  // the red: the gate bypassed and the Courant number forced to 1.3 × the bar (2.5): the top mode diverges
  const red = await offlineRun({ params: { ...DRUM, decay_s: 0, drum_cfl_gate: 0, drum_cfl: 1.3 }, script: [[0, 0x91, note, 127]], seconds: 2.5, snapHz: 20 });
  return { checks: [
    { name: 'the gong\'s swell: struck at velocity 127 the sheet lands sharp (the stretching raises c²) and settles on the note within a second (C3, swell 0.8)', pass: gHardEarly > 40 && Math.abs(gHardLate) < 15, detail: `${signed(gHardEarly)} cents in the first 300 ms, ${signed(gHardLate)} cents at 1.2–1.9 s (the book: a hundred cents sharp, then on the note)` },
    { name: 'the same gong at velocity 30 stays near the note from the start', pass: Math.abs(gSoftEarly) < 15, detail: `${signed(gSoftEarly)} cents in the first 300 ms` },
    { name: 'the drum (no swell) strikes on the note at full velocity', pass: Math.abs(dEarly) < 15, detail: `${signed(dEarly)} cents at velocity 127` },
    { name: 'the leapfrog keeps the sheet\'s invariant energy (the drum lossless, 2.7 s)', pass: drift < 0.05, detail: `${drift.toExponential(2)} dB over ${hs.length} snapshots, λ₂D at E_max ${lam.toFixed(3)} (the bar 1.95)` },
    { name: 'the red control: the CFL gate bypassed and λ₂D forced to 1.3 × the bar — the sheet diverges and the lab catches it', pass: red.blowups.length > 0, detail: red.blowups.length ? `caught at ${(red.blowups[0].block * 128 / sr).toFixed(2)} s, the engine restarted ${red.restarts.length}×` : 'not caught' },
  ], data: { gHardEarly, gHardLate, gSoftEarly, dEarly, drift, lam } };
}

// ---- the live page ----
async function hold(note, velocity) { await ensureStarted(lab); lab.velocity = velocity ?? +$('velocity').value; lab.hold(note); markKeys($('keys'), note, true); }
function release() { lab.release(); markKeys($('keys'), 0, false); }
const out = (el, d = 2) => { el.nextElementSibling.value = (+el.value).toFixed(d); };
async function apply(values) { const r = await lab.setParams(values); if (!r.ok) banner('refused', `The engine refused the patch: ${r.log.trim()}`); else if (!$('red').checked) banner(); return r; }

function drawSheet() {
  const { g, w, h } = fit($('sheet'));
  g.clearRect(0, 0, w, h);
  const ins = S.ins;
  if (!ins || !ins.n) { g.fillStyle = css('--muted'); g.font = '12px system-ui, sans-serif'; g.textAlign = 'center'; g.fillText(lab.ctx ? 'Strike a key — the sheet appears when it sounds.' : 'Start the sound, then strike a key.', w / 2, h / 2); return; }
  const nx = Math.round(ins.k[0]), ny = Math.round(ins.k[1]); if (nx < 1 || ny < 1) return;
  let peak = 1e-9; for (let i = 0; i < ins.n; i++) peak = Math.max(peak, Math.abs(ins.a[i]));
  S.uScale = Math.max(peak, S.uScale * 0.96);
  const side = Math.min(w, h) * 0.9, cw = side / (nx + 2), ch = side / (ny + 2), x0 = (w - side) / 2, y0 = (h - side) / 2;
  g.strokeStyle = css('--rule'); g.lineWidth = 1; g.strokeRect(x0 + cw * 0.5, y0 + ch * 0.5, side - cw, side - ch);   // the zero ring
  const ink = css('--trace');
  for (let j = 0; j < ny; j++) for (let i = 0; i < nx; i++) {
    const u = ins.a[j * nx + i] / S.uScale;
    g.fillStyle = u >= 0 ? ink : css('--env'); g.globalAlpha = clamp(Math.abs(u), 0.04, 1);
    g.fillRect(x0 + cw * (i + 1), y0 + ch * (j + 1), cw * 0.92, ch * 0.92);
  }
  g.globalAlpha = 1;
  g.fillStyle = css('--muted'); g.font = '11px system-ui, sans-serif'; g.textAlign = 'left'; g.fillText(`${nx} × ${ny} interior nodes`, x0 + 2, y0 - 4 + ch * 0.5);
}

function wire() {
  navigation('membrane'); $('chapter-link').href = `${DOCS_ROOT}suzu/membranes/`;
  const strobe = new Portrait($('strobe')), hist = new Strip($('history'), 6);
  lab.on('ready', () => { $('engine-line').textContent = `Voxo ${lab.version}, the membrane (voice kind ${KIND}), ${lab.sampleRate} Hz`; if (QS.has('demo')) { gong(); hold(48, 127); } });
  lab.on('snap', (m) => {
    S.ins = m.inspect.find((v) => v.held) || m.inspect[m.inspect.length - 1] || null;
    if (S.ins) {
      if (S.ins.serial !== S.serial) { S.serial = S.ins.serial; S.h0 = S.ins.k[7]; }
      S.hNow = S.ins.k[7];
    }
    if (m.strobe) for (const r of m.strobe) for (let i = 0; i < r.count; i++) S.dots.push([r.x[i], r.y[i]]);
  });
  lab.on('blowup', (m) => { banner('red', `It blew up ${(m.block * 128 / lab.sampleRate).toFixed(2)} s into the engine's life: with the gate bypassed the Courant number sat past the leapfrog's bound, so the top mode grew until its samples were no longer numbers. The lab muted it and restarted the engine — switch the red control off to play again.`); });
  buildKeys($('keys'), { lo: 36, hi: 72, onDown: (m) => hold(m) });
  bindKeymap(48, (m) => hold(m));
  bindPower(lab, $('power'));
  $('volume').addEventListener('input', (e) => lab.gain(+e.target.value));
  for (const id of ['alpha', 'kappa', 'cap', 'decay']) out($(id));
  for (const id of ['size', 'velocity']) out($(id), 0);
  $('alpha').addEventListener('input', (e) => { out(e.target); apply({ drum_alpha: +e.target.value }); });
  $('size').addEventListener('input', (e) => { out(e.target, 0); apply({ drum_size: +e.target.value }); });
  $('kappa').addEventListener('input', (e) => { out(e.target); apply({ drum_kappa: +e.target.value }); });
  $('cap').addEventListener('input', (e) => { out(e.target); apply({ drum_cap: +e.target.value }); });
  $('decay').addEventListener('input', (e) => { out(e.target); apply({ decay_s: +e.target.value }); });
  $('velocity').addEventListener('input', (e) => { out(e.target, 0); lab.velocity = +e.target.value; });
  const gong = () => { $('alpha').value = GONG.drum_alpha; $('kappa').value = GONG.drum_kappa; $('cap').value = GONG.drum_cap; $('decay').value = GONG.decay_s; for (const id of ['alpha', 'kappa', 'cap', 'decay']) out($(id)); apply({ ...GONG }); };
  $('gong').addEventListener('click', gong);
  $('hard').addEventListener('click', () => hold(lab.held ? lab.note : 48, 127));
  $('stop').addEventListener('click', release);
  $('red').addEventListener('change', async (e) => {
    await apply(e.target.checked ? { drum_cfl_gate: 0, drum_cfl: 1.3 } : { drum_cfl_gate: 1, drum_cfl: 0 });
    if (e.target.checked) banner('red', 'The CFL gate is bypassed and λ₂D forced to 1.3 × the bar. Strike a key and watch the top mode grow: the leapfrog past |tr J| = 2 is a hyperbola, not an ellipse, and the lab catches the blow-up.'); else banner();
  });
  const tick = () => {
    lab.readAudio(); S.hz = lab.pitch();
    const f0 = noteHz(lab.note), reg = register(S.hz, f0), ins = S.ins;
    drawSheet();
    strobe.draw([S.dots.splice(0)], { fade: 0.01, dots: true, xLabel: 'u', yLabel: 'p' });
    drawSpectrum($('spectrum'), lab, { f0, sounding: S.hz });
    $('r-note').textContent = `${noteName(lab.note)} · ${f0.toFixed(1)} Hz`;
    $('r-hz').textContent = reg ? `${S.hz.toFixed(1)} Hz` : (lab.held ? 'no single pitch' : '—');
    $('r-cents').textContent = reg ? `${signed(1200 * Math.log2(S.hz / f0))} cents` : '—';
    $('r-cfl').textContent = ins ? `${ins.k[4].toFixed(3)} (the bar 1.95)` : '—';
    $('r-e').textContent = ins && ins.k[11] > 0 ? (ins.k[5] / ins.k[11]).toFixed(3) : '—';
    const rel = ins && S.h0 > 0 ? dB(S.hNow / S.h0) : null;
    $('r-h').textContent = rel === null ? '—' : `${signed(rel, 3)} dB`;
    hist.push({ h: rel });
    hist.draw([{ key: 'h', color: css('--trace'), lo: -60, hi: 10, width: 2 }], [[0, 'the strike', 'h'], [-30, '−30 dB', 'h']]);
    footer(lab, ins && ins.k[6] > 0 ? 'gong' : 'membrane');
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
  if (QS.has('demo')) ensureStarted(lab);
}

if (QS.get('gate') === 'membrane') gateGuard('membrane', gate); else wire();
