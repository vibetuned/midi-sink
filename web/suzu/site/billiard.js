// billiard.js — the Suzu lab's billiard (v3.0 step 92; the voice of step 85, DECISIONS_13 #3; TO_PROJECT_SPEC §13.13).
//
// Voice kind 13: an oscillator inside a stadium — the exact rotation between collisions, the specular reflection at
// the crossing's fraction of the step with the two-point polyBLAMP of each kink. A radial orbit kicked v past the
// wall bounces at (π/2)/asin(1/v) times the note; the wheel pulls the straight walls out and the orbit is ergodic.
// ?gate=billiard checks the note under the wall, the bounce by the geometry, the strobe's line on the circle against
// its dust on the stadium, and the red control — the reflection at the step's end — 3.8 % off the geometry.
import { QS, $, css, noteName, noteHz, clamp, Lab, bindPower, ensureStarted, buildKeys, markKeys, bindKeymap, drawSpectrum,
  Portrait, Strip, banner, footer, navigation, offlineRun, pitchAt, gateGuard, register, signed, fit, DOCS_ROOT } from './lab-core.js';

const KIND = 13;
// the suite's circle (tests/voxo_suzu_tests.cpp gate 61): the patch table's billiard with the drive at 2.5 and the walls in
const BASE = { voice_kind: KIND, bill_drive: 2.5, bill_aspect: 1, bill_length: 2, bill_length_rest: 0, bill_exact: 1, decay_s: 2, pluck: 0.5,
  level: 0.3, attack_s: 0.002, release_s: 0.5, cutoff_hz: 20000, resonance: 0 };
const lab = new Lab({ params: { ...BASE }, traceMask: 1 << KIND, traceDecim: 2, recentPoints: 1023, snapHz: 60, strobe: 1 });
const S = { ins: null, pts: [], dots: [], cells: new Set(), hz: 0 };
const bounce = (v) => (v > 1 ? (Math.PI / 2) / Math.asin(1 / v) : 1);
const cellsOf = (pts, range) => { const s = new Set(); for (const [x, y] of pts) { const i = clamp(Math.floor((x / range + 1) * 12), 0, 23), j = clamp(Math.floor((y / range + 1) * 12), 0, 23); s.add(i * 24 + j); } return s.size; };

// ---- the browser check ----
async function gate() {
  const sr = 48000, note = 57, f0 = noteHz(note);
  const run = (p, seconds, strobe = 0) => offlineRun({ params: p, script: [[0, 0x91, note, 127]], seconds, snapHz: 20, traceMask: 1 << KIND, traceDecim: 4, strobe });
  // (1) under one wall-width: a sine at the note
  const u = await run({ ...BASE, bill_drive: 0.6, decay_s: 0 }, 1.5);
  const uC = 1200 * Math.log2(pitchAt(u.x, sr, 1.0) / f0);
  // (2) the fold: the circle kicked past the wall bounces by the geometry, v from the engine's H = v²/2 at the kick
  const geo = async (drive) => { const r = await run({ ...BASE, bill_drive: drive, decay_s: 0 }, 1.5, 1); const k = r.snaps.find((s) => s.inspect.length)?.inspect[0].k; const v = k ? Math.sqrt(2 * k[4]) : 0; return { r, v, pred: bounce(v), meas: pitchAt(r.x, sr, 1.0) / f0, hits: r.snaps.filter((s) => s.inspect.length).pop()?.inspect[0].k[5] ?? 0 }; };
  const g25 = await geo(2.5), g175 = await geo(1.75);
  const off25 = Math.abs(g25.meas / g25.pred - 1), off175 = Math.abs(g175.meas / g175.pred - 1);
  // (3) the strobe: the circle's returns on a line, the stadium's as dust (the bass phasor's clock)
  const strobePts = (r) => { const pts = []; for (const s of r.snaps) if (s.strobe) for (const rec of s.strobe) for (let i = 0; i < rec.count; i++) pts.push([rec.x[i], rec.y[i]]); return pts; };
  const st = await run({ ...BASE, bill_length_rest: 2, decay_s: 0 }, 1.5, 1);
  const pc = strobePts(g25.r), ps = strobePts(st);
  let range = 1e-9; for (const [x, y] of [...pc, ...ps]) range = Math.max(range, Math.abs(x), Math.abs(y));
  const cCircle = cellsOf(pc, range * 1.01), cStadium = cellsOf(ps, range * 1.01);
  // (4) the red: the reflection at the step's end — the bounce quantised to whole steps; the quantum is a bigger share
  // of the bounce as the note rises (the suite's 3.8 % is its worst over A3, A4 and A5), so the red is read at A5 too
  const redAt = async (n) => { const fn = noteHz(n); const r = await offlineRun({ params: { ...BASE, bill_exact: 0, decay_s: 0 }, script: [[0, 0x91, n, 127]], seconds: 1.5, snapHz: 20 }); const k = r.snaps.find((s) => s.inspect.length)?.inspect[0].k, v = k ? Math.sqrt(2 * k[4]) : 0; return { v, off: Math.abs(pitchAt(r.x, sr, 1.0) / fn / bounce(v) - 1) }; };
  const r57 = await redAt(57), r81 = await redAt(81), redOff = Math.max(r57.off, r81.off), rv = r81.v;
  return { checks: [
    { name: 'under one wall-width the voice is a sine at the note (drive 0.6, A3)', pass: Number.isFinite(uC) && Math.abs(uC) < 15, detail: `${signed(uC, 1)} cents` },
    { name: 'the fold: the circle kicked past the wall bounces at (π/2)/asin(1/v) times the note (the book: 2.58× at v = 1.75)', pass: off25 < 0.01 && off175 < 0.01 && g25.hits > 50, detail: `v ${g25.v.toFixed(2)}: ${g25.meas.toFixed(3)}× against ${g25.pred.toFixed(3)}× (${(100 * off25).toFixed(2)} %, ${g25.hits} collisions); v ${g175.v.toFixed(2)}: ${g175.meas.toFixed(3)}× against ${g175.pred.toFixed(3)}× (${(100 * off175).toFixed(2)} %)` },
    { name: 'the strobe: on the circle the returns stay on a line (a central kick is radial); on the stadium they spread as dust (the suite: 24 against 153 of 576 cells)', pass: pc.length > 50 && cCircle <= 60 && cStadium > 2 * cCircle, detail: `${cCircle} cells of 576 on the circle (${pc.length} returns), ${cStadium} on the stadium (${ps.length})` },
    { name: 'the red control: the reflection at the step\'s end quantises the bounce to whole steps — off the geometry by percent (the suite: 3.8 % at worst over A3–A5) where the exact path is within a hundredth', pass: redOff > 0.015 && off25 < 0.01, detail: `${(100 * r57.off).toFixed(2)} % off at A3, ${(100 * r81.off).toFixed(2)} % at A5 (v ${rv.toFixed(2)}; the exact ${(100 * off25).toFixed(2)} %)` },
  ], data: { uC, off25, off175, cCircle, cStadium, redOff } };
}

// ---- the live page ----
async function hold(note) { await ensureStarted(lab); lab.velocity = +$('velocity').value; lab.hold(note); markKeys($('keys'), note, true); }
function release() { lab.release(); markKeys($('keys'), 0, false); }
const out = (el, d = 2) => { el.nextElementSibling.value = (+el.value).toFixed(d); };
async function apply(values) { const r = await lab.setParams(values); if (!r.ok) banner('refused', `The engine refused the patch: ${r.log.trim()}`); else if (!$('red').checked) banner(); return r; }

function drawTable() {
  const { g, w, h } = fit($('table'));
  g.clearRect(0, 0, w, h);
  const ins = S.ins, L = ins ? ins.k[1] : 0, a = ins ? ins.k[2] : 1, b = ins ? ins.k[3] : 1;
  const ext = Math.max(a + L / 2, b, 1), sc = Math.min(w, h) * 0.44 / ext, cx = w / 2, cy = h / 2;
  g.strokeStyle = css('--rule'); g.lineWidth = 1.5; g.beginPath();
  g.ellipse(cx + (L / 2) * sc, cy, a * sc, b * sc, 0, -Math.PI / 2, Math.PI / 2);
  g.ellipse(cx - (L / 2) * sc, cy, a * sc, b * sc, 0, Math.PI / 2, 3 * Math.PI / 2);
  g.closePath(); g.stroke();
  g.strokeStyle = css('--trace'); g.lineWidth = 1.2; g.beginPath();
  for (let i = 0; i < S.pts.length; i++) { const [x, y] = S.pts[i]; if (i === 0) g.moveTo(cx + x * sc, cy - y * sc); else g.lineTo(cx + x * sc, cy - y * sc); }
  g.stroke();
  g.fillStyle = css('--muted'); g.font = '11px system-ui, sans-serif'; g.textAlign = 'center';
  g.fillText(ins ? `L ${L.toFixed(2)} · a ${a.toFixed(2)} · b ${b.toFixed(2)} (in the wall's half-height)` : (lab.ctx ? 'Strike a key — the table appears when it sounds.' : 'Start the sound, then strike a key.'), cx, h - 6);
}

function wire() {
  navigation('billiard'); $('chapter-link').href = `${DOCS_ROOT}suzu/billiard/`;
  const strobe = new Portrait($('strobe')), hist = new Strip($('history'), 6);
  lab.on('ready', () => { $('engine-line').textContent = `Voxo ${lab.version}, the billiard (voice kind ${KIND}), ${lab.sampleRate} Hz`; lab.cc(1, +$('wheel').value); lab.cc(74, +$('aspect').value); if (QS.has('demo')) hold(57); });
  lab.on('snap', (m) => {
    S.ins = m.inspect.find((v) => v.held) || m.inspect[m.inspect.length - 1] || null;
    if (m.recent && m.recent.length >= 8 && S.ins) { const r = m.recent, pts = []; for (let i = 0; i < r.length; i += 4) pts.push([r[i], r[i + 1]]); S.pts = pts.slice(-600); } else if (!S.ins) S.pts = [];
    if (m.strobe) for (const r of m.strobe) for (let i = 0; i < r.count; i++) { S.dots.push([r.x[i], r.y[i]]); S.cells.add(clamp(Math.floor((r.x[i] / 2.2 + 1) * 12), 0, 23) * 24 + clamp(Math.floor((r.y[i] / 2.2 + 1) * 12), 0, 23)); }
  });
  buildKeys($('keys'), { lo: 36, hi: 84, onDown: (m) => hold(m) });
  bindKeymap(57, (m) => hold(m));
  bindPower(lab, $('power'));
  $('volume').addEventListener('input', (e) => lab.gain(+e.target.value));
  out($('drive')); out($('decay')); for (const id of ['wheel', 'aspect', 'velocity']) out($(id), 0);
  $('drive').addEventListener('input', (e) => { out(e.target); apply({ bill_drive: +e.target.value }); });
  $('wheel').addEventListener('input', (e) => { out(e.target, 0); lab.cc(1, +e.target.value); });
  $('aspect').addEventListener('input', (e) => { out(e.target, 0); lab.cc(74, +e.target.value); });
  $('decay').addEventListener('input', (e) => { out(e.target); apply({ decay_s: +e.target.value }); });
  $('velocity').addEventListener('input', (e) => { out(e.target, 0); lab.velocity = +e.target.value; });
  $('clear').addEventListener('click', () => { S.dots = []; S.cells.clear(); strobe.clear(); });
  $('stop').addEventListener('click', release);
  $('red').addEventListener('change', async (e) => {
    await apply({ bill_exact: e.target.checked ? 0 : 1 });
    if (e.target.checked) banner('red', 'The reflection happens at the step\'s end, not at its fraction, and without the polyBLAMP: the bounce is quantised to whole steps — 3.8 % off the geometry on the circle, where the exact path is within a hundredth of a percent.'); else banner();
  });
  const tick = () => {
    lab.readAudio(); S.hz = lab.pitch();
    const f0 = noteHz(lab.note), reg = register(S.hz, f0), ins = S.ins, v = ins ? Math.sqrt(2 * Math.max(ins.k[4], 0)) : 0;
    drawTable();
    strobe.draw([S.dots.splice(0)], { fade: 0.004, dots: true, fixed: 2.2, xLabel: 'x₁', yLabel: 'x₂' });
    drawSpectrum($('spectrum'), lab, { f0, sounding: S.hz });
    $('r-note').textContent = `${noteName(lab.note)} · ${f0.toFixed(1)} Hz`;
    $('r-hz').textContent = reg ? `${S.hz.toFixed(1)} Hz` : (lab.held ? 'no single pitch' : '—');
    $('r-ratio').textContent = reg ? `× ${(S.hz / f0).toFixed(3)}` : '—';
    $('r-pred').textContent = ins ? (v > 1 ? `× ${bounce(v).toFixed(3)} at v ${v.toFixed(2)}` : `under the wall (v ${v.toFixed(2)})`) : '—';
    $('r-hits').textContent = ins ? `${ins.k[5]}` : '—';
    $('r-cells').textContent = S.cells.size ? `${S.cells.size}` : '—';
    hist.push({ r: reg ? S.hz / f0 : null });
    hist.draw([{ key: 'r', color: css('--trace'), lo: 0.5, hi: 4.5, width: 2 }], [[1, 'the note', 'r'], [2, '×2', 'r'], [3, '×3', 'r'], [4, '×4', 'r']]);
    footer(lab, 'billiard');
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
  if (QS.has('demo')) ensureStarted(lab);
}

if (QS.get('gate') === 'billiard') gateGuard('billiard', gate); else wire();
