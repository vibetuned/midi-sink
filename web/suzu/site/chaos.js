// chaos.js — the Suzu lab's Duffing cell and kicked rotor (Phase 8 step 59c, DECISIONS_7 #31; SYNTH §2.3, §2.10).
//
// Voice kind 5: the standard map as an oscillator — p += K·sin θ at each kick, the cell retuned to f₀(1 + p/2π), so
// θ advances by p: the section (θ, p) at the kicks, built here from the ring's momentum channel, IS Chirikov's map.
// Voice kind 4: the hardening spring — the clang (pitch against time after a hard strike) and, driven by the press,
// its stroboscopic section (the state where the drive's phase wraps). ?gate=chaos checks the rotor's momentum confined
// below K_c (the primary island) and spread into the sea past it, and the Duffing clang's settle.
import { QS, $, css, noteName, noteHz, clamp, Lab, bindPower, ensureStarted, showNote, buildKeys, markKeys, bindKeymap, drawSpectrum,
  Portrait, Strip, banner, footer, navigation, offlineRun, at, pitchAt, gateGuard, register, signed, DOCS_ROOT } from './lab-core.js';

const PATCH = { 5: { voice_kind: 5, rotor_k: 0, decay_s: 0 }, 4: { voice_kind: 4, duffing_beta: 8, drive: 0.3, drive_ratio: 1, decay_s: 3 } };
const NOTES = {
  5: 'Each dot is one kick: the angle θ across, the momentum p up, both on the circle (−π … π). Below K ≈ 0.97 the dots lie on curves — the tori that order holds to; past it the last torus breaks and they fill a chaotic sea, the pitch wandering with them.',
  4: 'Drive it with the press and each dot is the cell\'s state once per drive period: a single point when it follows, a few for a subharmonic, a cloud when the drive wins — the order-to-chaos road of a forced, hardening spring. Undriven, the section is empty; strike it hard and watch the pitch fall back to the note.',
};
const lab = new Lab({ params: { ...PATCH[5] }, traceMask: (1 << 4) | (1 << 5), traceDecim: 2, recentPoints: 1023, snapHz: 60 });
const S = { kind: 5, pts: [], dots: [], last: null, ps: [], hz: 0, ins: null, sweep: null };

// the section's dots from a run of recent points (x, y, u, w): the rotor at each kick (p changed), Duffing where the phase wraps
function sectionDots(r, kind, last) {
  const dots = []; let prev = last;
  for (let i = 0; i < r.length; i += 4) {
    const x = r[i], y = r[i + 1], u = r[i + 2];
    if (prev) {
      if (kind === 5 && u !== prev[2]) dots.push([Math.atan2(prev[1], prev[0]) / Math.PI, u / Math.PI]);
      if (kind === 4 && u < prev[2] - Math.PI) dots.push([x, y]);
    }
    prev = [x, y, u];
  }
  return { dots, last: prev };
}
const std = (a) => { if (!a.length) return NaN; const m = a.reduce((s, v) => s + v, 0) / a.length; return Math.sqrt(a.reduce((s, v) => s + (v - m) ** 2, 0) / a.length); };

// ---- the browser check ----
async function gate() {
  const note = 57, f0 = noteHz(note), sr = 48000;
  const r = await offlineRun({ params: { ...PATCH[5] }, script: [[0, 0xB0, 1, 15], [0, 0x91, note, 100], [at(sr, 3), 0xB0, 1, 127]], seconds: 6, traceMask: 1 << 5, traceDecim: 4, recentPoints: 1023, snapHz: 30 });
  const low = [], high = []; let last = null;
  for (const s of r.snaps) {
    if (!s.recent) continue;
    const t = s.block * 128 / sr, d = sectionDots(s.recent, 5, last); last = d.last;
    for (const [, p] of d.dots) { if (t >= 1.5 && t <= 3) low.push(p * Math.PI); if (t >= 4.5) high.push(p * Math.PI); }
  }
  const sl = std(low), sh = std(high), ml = Math.max(...low.map(Math.abs)), mh = Math.max(...high.map(Math.abs));
  const d = await offlineRun({ params: { ...PATCH[4], drive: 0 }, script: [[0, 0x91, note, 127]], seconds: 2.2 });
  const early = register(pitchAt(d.x, sr, 0.08, 2048), f0), late = register(pitchAt(d.x, sr, 2.0), f0);
  return { checks: [
    { name: 'the rotor below K_c: the tori confine the momentum to the primary island (K ≈ 0.30, its half-width 2√K ≈ 1.1)', pass: low.length > 100 && ml < 1.6, detail: `|p| at most ${ml.toFixed(2)} rad over ${low.length} kicks (its spread ${sl.toFixed(2)})` },
    { name: 'the rotor past K_c: the momentum spreads into the chaotic sea (K = 2.5; its spread near a uniform sea\'s and more than twice the confined one)', pass: high.length > 100 && sh > 1.4 && sh > 2 * sl, detail: `its spread ${sh.toFixed(2)} rad over ${high.length} kicks (a uniform sea: π/√3 = 1.81; confined: ${sl.toFixed(2)}), |p| up to ${mh.toFixed(2)} — the period-1 island about θ = π stays stable until K = 4 and the sea surrounds it` },
    { name: 'the Duffing clang: struck hard it sounds sharp and settles onto the note (β 8, A3, velocity 127)', pass: !!early && early.cents > 80 && !!late && Math.abs(late.cents) < 25, detail: `${early ? signed(early.cents) : '—'} cents in the first 80 ms, ${late ? signed(late.cents, 1) : '—'} cents at 2 s` },
  ], data: { sl, sh } };
}

// ---- the live page ----
async function hold(note, velocity) { await ensureStarted(lab); lab.velocity = velocity ?? +$('velocity').value; lab.hold(note); S.last = null; markKeys($('keys'), note, true); }
function release() { lab.release(); markKeys($('keys'), 0, false); }
const out = (el, d = 2) => { el.nextElementSibling.value = (+el.value).toFixed(d); };
async function apply(values) { const r = await lab.setParams(values); banner('refused', r.ok ? '' : `The engine refused the patch: ${r.log.trim()}`); return r; }
const kOf = (cc) => 2.5 * cc / 127;
async function setKind(k) {
  S.kind = k; S.dots = []; S.ps = []; S.last = null; section.clear();
  for (const b of $('kind').querySelectorAll('button')) b.classList.toggle('on', +b.dataset.k === k);
  for (const el of document.querySelectorAll('.only-5')) el.hidden = k !== 5;
  for (const el of document.querySelectorAll('.only-4')) el.hidden = k !== 4;
  $('section-title').textContent = k === 5 ? 'The Chirikov section (θ, p) at each kick' : 'The stroboscopic section';
  $('section-note').textContent = NOTES[k];
  $('r-a-label').textContent = k === 5 ? 'K' : 'β';
  $('r-b-label').textContent = k === 5 ? 'The momentum\'s spread' : 'The drive';
  const p = k === 5 ? { ...PATCH[5] } : { ...PATCH[4], duffing_beta: +$('beta').value, drive: +$('drive').value, drive_ratio: +$('ratio').value };
  const r = await apply(p);
  if (r.ok && lab.held) hold(lab.note);
}
function sweep() {
  if (S.sweep) { clearInterval(S.sweep); S.sweep = null; $('sweep').textContent = 'Sweep K to 2.5'; return; }
  if (!lab.held) hold(57);
  const t0 = performance.now(), from = +$('k').value; $('sweep').textContent = 'Stop the sweep';
  S.sweep = setInterval(() => { const v = Math.round(from + (127 - from) * Math.min(1, (performance.now() - t0) / 12000)); $('k').value = v; out($('k'), 0); lab.cc(1, v, 0); if (v >= 127) sweep(); }, 40);
}

const section = new Portrait($('section'));
function wire() {
  navigation('chaos'); $('chirikov-link').href = `${DOCS_ROOT}operators/chirikov/`;
  const orbit = new Portrait($('orbit')), hist = new Strip($('history'), 6);
  lab.on('ready', () => { $('engine-line').textContent = `Voxo ${lab.version}, the chaos voices, ${lab.sampleRate} Hz`; lab.cc(1, +$('k').value, 0); if (QS.has('demo')) { $('k').value = 100; out($('k'), 0); lab.cc(1, 100, 0); hold(57); } });
  lab.on('snap', (m) => {
    S.ins = m.inspect.find((v) => v.held) || m.inspect[m.inspect.length - 1] || null;
    if (m.recent && m.recent.length >= 8 && S.ins) {
      const r = m.recent, pts = []; for (let i = 0; i < r.length; i += 4) pts.push([r[i], r[i + 1]]);
      S.pts = pts.slice(-Math.round(2 * S.ins.rate2 / 2 / Math.max(S.ins.freq, 1)));
      const d = sectionDots(r, S.kind, S.last); S.last = d.last; S.dots.push(...d.dots);
      if (S.kind === 5) { for (const [, p] of d.dots) S.ps.push(p * Math.PI); if (S.ps.length > 400) S.ps.splice(0, S.ps.length - 400); }
    } else if (!S.ins) S.pts = [];
  });
  buildKeys($('keys'), { lo: 36, hi: 84, onDown: (m) => hold(m) });
  bindKeymap(60, (m) => hold(m));
  bindPower(lab, $('power'));
  $('volume').addEventListener('input', (e) => lab.gain(+e.target.value));
  for (const id of ['k', 'press', 'velocity']) out($(id), 0);
  for (const id of ['beta', 'drive', 'ratio']) out($(id));
  for (const b of $('kind').querySelectorAll('button')) b.addEventListener('click', () => setKind(+b.dataset.k));
  $('k').addEventListener('input', (e) => { out(e.target, 0); lab.cc(1, +e.target.value, 0); });
  $('sweep').addEventListener('click', sweep);
  $('clear').addEventListener('click', () => { S.dots = []; S.ps = []; section.clear(); });
  $('beta').addEventListener('input', (e) => { out(e.target); apply({ duffing_beta: +e.target.value }); });
  $('drive').addEventListener('input', (e) => { out(e.target); apply({ drive: +e.target.value }); });
  $('ratio').addEventListener('input', (e) => { out(e.target); apply({ drive_ratio: +e.target.value }); });
  $('press').addEventListener('input', (e) => { out(e.target, 0); lab.pressure(+e.target.value); S.dots = []; section.clear(); });
  $('hard').addEventListener('click', () => hold(lab.held || lab.note !== 60 ? lab.note : 57, 127));
  $('velocity').addEventListener('input', (e) => { out(e.target, 0); lab.velocity = +e.target.value; });
  $('stop').addEventListener('click', release);
  setKind(5);
  const tick = () => {
    lab.readAudio(); S.hz = lab.pitch();
    const f0 = noteHz(lab.note), reg = register(S.hz, f0), ins = S.ins;
    section.draw([S.dots.splice(0)], { fade: S.kind === 5 ? 0.004 : 0.01, dots: true, fixed: S.kind === 5 ? 1 : 0, xLabel: S.kind === 5 ? 'θ' : 'x', yLabel: S.kind === 5 ? 'p' : 'y' });
    orbit.draw([S.pts], { fade: 0.14, xLabel: 'x', yLabel: 'y' });
    drawSpectrum($('spectrum'), lab, { f0, sounding: S.hz });
    $('r-note').textContent = `${noteName(lab.note)} · ${f0.toFixed(1)} Hz`;
    $('r-hz').textContent = reg ? `${S.hz.toFixed(1)} Hz` : (lab.held ? 'no single pitch' : '—');
    $('r-cents').textContent = reg ? `${signed(1200 * Math.log2(S.hz / f0))} cents` : '—';
    $('r-a').textContent = S.kind === 5 ? kOf(+$('k').value).toFixed(2) + (kOf(+$('k').value) > 0.9716 ? ' (past K_c)' : '') : (+$('beta').value).toFixed(1);
    $('r-b').textContent = S.kind === 5 ? (S.ps.length > 20 ? `${std(S.ps).toFixed(2)} rad` : '—') : `${$('press').value}/127 × ${(+$('drive').value).toFixed(2)}`;
    hist.push({ cents: S.hz > 0 ? 1200 * Math.log2(S.hz / f0) : null });
    hist.draw([{ key: 'cents', color: css('--trace'), lo: -700, hi: 900, width: 2 }], [[0, 'the note', 'cents'], [700, '+700', 'cents'], [-700, '−700', 'cents']]);
    footer(lab, S.kind === 5 ? 'kicked rotor' : 'Duffing cell');
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
  if (QS.has('demo')) ensureStarted(lab);
}

if (QS.get('gate') === 'chaos') gateGuard('chaos', gate); else wire();
