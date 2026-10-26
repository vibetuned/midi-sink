// gyro.js — the Suzu lab's Boris gyroscope (v3.0 step 92; the voice of step 84, DECISIONS_13 #2; TO_PROJECT_SPEC §13.13).
//
// Voice kind 11: a particle on a spring in a magnetic field, Boris's leapfrog with the Cayley rotation as three exact
// shears; (x₁, x₂) are the left and right channels. The note splits into the Zeeman doublet ω± = √(ω₀² + (ω_B/2)²) ± ω_B/2,
// sounded exactly when both the spring and the field are prewarped. ?gate=gyro checks the doublet against the formula,
// the mono line without a field, the energy held, and the red control — the prewarp off — 1.21 cents sharp at B6.
import { QS, $, css, noteName, noteHz, Lab, bindPower, ensureStarted, buildKeys, markKeys, bindKeymap, drawSpectrum,
  Portrait, Strip, banner, footer, navigation, offlineRun, gateGuard, signed, peakScan, DOCS_ROOT } from './lab-core.js';

const KIND = 11, CH = 1;
// the patch table's gyroscope (voxo/src/suzu_patches.cpp)
const BASE = { voice_kind: KIND, boris_b: 24, boris_b_rest: 2, boris_prewarp: 1, decay_s: 4, pluck: 0.5,
  level: 0.3, attack_s: 0.002, release_s: 0.5, cutoff_hz: 20000, resonance: 0 };
const lab = new Lab({ params: { ...BASE }, traceMask: 1 << KIND, traceDecim: 2, recentPoints: 1023, snapHz: 60, strobe: 2 });
const S = { ins: null, pts: [], dots: [], serial: -1, h0: 0, hNow: 0, press: 64 };
const dB = (r) => 10 * Math.log10(Math.max(r, 1e-30));
const doublet = (f0, B) => { const c = Math.sqrt(f0 * f0 + (B / 2) * (B / 2)); return [c + B / 2, c - B / 2]; };
const cents = (a, b) => 1200 * Math.log2(a / b);

// ---- the browser check ----
async function gate() {
  const sr = 48000;
  const scan = (x, f, half, step = 0.02) => peakScan(x.subarray(Math.floor(1.0 * sr), Math.floor(3.0 * sr)), sr, f - half, f + half, step).hz;
  // (1) the doublet at A4 under the full field (24 Hz, the poly pressure)
  const f0 = noteHz(69), [fp, fm] = doublet(f0, 24);
  const d = await offlineRun({ params: { ...BASE, boris_b_rest: 0, decay_s: 0 }, script: [[0, 0x91, 69, 100], [0, 0xA1, 69, 127]], seconds: 3.0, snapHz: 10 });
  const mp = scan(d.x, fp, 3), mm = scan(d.x, fm, 3), ep = cents(mp, fp), em = cents(mm, fm);
  const dk = d.snaps.filter((s) => s.inspect.length).pop()?.inspect[0].k ?? null;
  // (2) no field: the one line on the note
  const m = await offlineRun({ params: { ...BASE, boris_b_rest: 0, decay_s: 0 }, script: [[0, 0x91, 69, 100]], seconds: 3.0 });
  const e0 = cents(scan(m.x, f0, 3), f0);
  // (3) H held lossless under the field
  const hs = d.snaps.filter((s) => s.inspect.length && s.block * 128 / sr > 0.3).map((s) => s.inspect[0].k[2]);
  const drift = hs.length > 4 ? Math.abs(dB(hs[hs.length - 1] / hs[0])) : Infinity;
  // (4) the red: the prewarp off at B6 — the raw ε = ω₀dt puts the line (ω₀dt)²/24 sharp: 1.21 cents at B6
  const fB6 = noteHz(95);
  const raw = await offlineRun({ params: { ...BASE, boris_b_rest: 0, boris_prewarp: 0, decay_s: 0 }, script: [[0, 0x91, 95, 100]], seconds: 3.0 });
  const pre = await offlineRun({ params: { ...BASE, boris_b_rest: 0, decay_s: 0 }, script: [[0, 0x91, 95, 100]], seconds: 3.0 });
  const eRaw = cents(scan(raw.x, fB6, 8), fB6), ePre = cents(scan(pre.x, fB6, 8), fB6);
  return { checks: [
    { name: 'the Zeeman doublet: under a 24 Hz field A4 splits into two lines ω_B apart, at √(ω₀² + (ω_B/2)²) ± ω_B/2 (the left channel, scanned to a fiftieth of a hertz)', pass: Math.abs(ep) < 0.5 && Math.abs(em) < 0.5, detail: `${mp.toFixed(2)} and ${mm.toFixed(2)} Hz against ${fp.toFixed(2)} and ${fm.toFixed(2)}: ${signed(ep, 2)} / ${signed(em, 2)} cents${dk ? ` (the engine's own doublet ${dk[4].toFixed(2)} / ${dk[5].toFixed(2)} Hz, the field ${dk[3].toFixed(1)} Hz)` : ''}` },
    { name: 'no field: one line, on the note', pass: Math.abs(e0) < 0.3, detail: `${signed(e0, 2)} cents` },
    { name: 'the Cayley rotation keeps H (lossless, under the field, 2.7 s)', pass: drift < 0.05, detail: `${drift.toExponential(2)} dB over ${hs.length} snapshots` },
    { name: 'the red control: the prewarp off puts the line sharp by (ω₀dt)²/24 — 1.21 cents at B6 — where the prewarped scheme sits on the note', pass: eRaw > 0.7 && eRaw < 1.8 && Math.abs(ePre) < 0.3, detail: `raw ${signed(eRaw, 2)} cents, prewarped ${signed(ePre, 2)} cents at B6` },
  ], data: { ep, em, e0, drift, eRaw, ePre } };
}

// ---- the live page ----
async function hold(note) { await ensureStarted(lab); lab.velocity = +$('velocity').value; lab.hold(note); lab.send([[0xA0 | CH, note, S.press]]); markKeys($('keys'), note, true); }
function release() { lab.release(); markKeys($('keys'), 0, false); }
const out = (el, d = 2) => { el.nextElementSibling.value = (+el.value).toFixed(d); };
async function apply(values) { const r = await lab.setParams(values); if (!r.ok) banner('refused', `The engine refused the patch: ${r.log.trim()}`); else if (!$('red').checked) banner(); return r; }

function wire() {
  navigation('gyro'); $('chapter-link').href = `${DOCS_ROOT}suzu/gyroscope/`;
  const spiro = new Portrait($('spiro')), strobe = new Portrait($('strobe')), hist = new Strip($('history'), 6);
  lab.on('ready', () => { $('engine-line').textContent = `Voxo ${lab.version}, the gyroscope (voice kind ${KIND}), ${lab.sampleRate} Hz`; if (QS.has('demo')) { $('press').value = 110; out($('press'), 0); S.press = 110; hold(69); } });
  lab.on('snap', (m) => {
    S.ins = m.inspect.find((v) => v.held) || m.inspect[m.inspect.length - 1] || null;
    if (S.ins) { if (S.ins.serial !== S.serial) { S.serial = S.ins.serial; S.h0 = S.ins.k[2]; } S.hNow = S.ins.k[2]; }
    if (m.recent && m.recent.length >= 8 && S.ins) { const r = m.recent, pts = []; for (let i = 0; i < r.length; i += 4) pts.push([r[i], r[i + 1]]); S.pts = pts; } else if (!S.ins) S.pts = [];
    if (m.strobe) for (const r of m.strobe) for (let i = 0; i < r.count; i++) S.dots.push([r.x[i], r.y[i]]);
  });
  buildKeys($('keys'), { lo: 48, hi: 96, onDown: (m) => hold(m) });
  bindKeymap(69, (m) => hold(m));
  bindPower(lab, $('power'));
  $('volume').addEventListener('input', (e) => lab.gain(+e.target.value));
  for (const id of ['bfull', 'brest']) out($(id), 1);
  out($('decay')); for (const id of ['press', 'velocity']) out($(id), 0);
  $('press').addEventListener('input', (e) => { out(e.target, 0); S.press = +e.target.value; if (lab.held) lab.send([[0xA0 | CH, lab.note, S.press]]); });
  $('bfull').addEventListener('input', (e) => { out(e.target, 1); apply({ boris_b: +e.target.value }); });
  $('brest').addEventListener('input', (e) => { out(e.target, 1); apply({ boris_b_rest: +e.target.value }); });
  $('decay').addEventListener('input', (e) => { out(e.target); apply({ decay_s: +e.target.value }); });
  $('velocity').addEventListener('input', (e) => { out(e.target, 0); lab.velocity = +e.target.value; });
  $('stop').addEventListener('click', release);
  $('red').addEventListener('change', async (e) => {
    await apply({ boris_prewarp: e.target.checked ? 0 : 1 });
    if (e.target.checked) banner('red', 'The prewarp is off: ε = ω₀dt and θ = ω_B dt, the small-angle coefficients. The doublet sits (ω₀dt)²/24 sharp — a hundredth of a cent at A3, 1.21 cents at B6 — and the energy is still kept: the error is in the pitch, never the stability.'); else banner();
  });
  const tick = () => {
    lab.readAudio();
    const f0 = noteHz(lab.note), ins = S.ins;
    spiro.draw([S.pts], { fade: 0.06, xLabel: 'x₁ (L)', yLabel: 'x₂ (R)' });
    strobe.draw([S.dots.splice(0)], { fade: 0.008, dots: true, xLabel: 'x₁', yLabel: 'x₂' });
    drawSpectrum($('spectrum'), lab, { f0, ratios: ins && ins.k[4] > 0 ? [ins.k[4] / f0, ins.k[5] / f0] : null, sounding: 0 });
    $('r-note').textContent = `${noteName(lab.note)} · ${f0.toFixed(1)} Hz`;
    $('r-field').textContent = ins ? `${ins.k[3].toFixed(2)} Hz` : '—';
    $('r-doublet').textContent = ins ? `${ins.k[4].toFixed(2)} / ${ins.k[5].toFixed(2)} Hz` : '—';
    $('r-cont').textContent = ins ? `${ins.k[6].toFixed(2)} / ${ins.k[7].toFixed(2)} Hz` : '—';
    const rel = ins && S.h0 > 0 ? dB(S.hNow / S.h0) : null;
    $('r-h').textContent = rel === null ? '—' : `${signed(rel, 3)} dB`;
    hist.push({ h: rel });
    hist.draw([{ key: 'h', color: css('--trace'), lo: -60, hi: 10, width: 2 }], [[0, 'the strike', 'h'], [-30, '−30 dB', 'h']]);
    footer(lab, 'Boris gyroscope');
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
  if (QS.has('demo')) ensureStarted(lab);
}

if (QS.get('gate') === 'gyro') gateGuard('gyro', gate); else wire();
