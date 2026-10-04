// modal.js — the Suzu lab's modal voice and bow (Phase 8 step 59c, DECISIONS_7 #31; SYNTH §2.5–§2.6).
//
// Voice kind 1: the modal lattice. The bars are the modes' energies from the inspection (a, b per
// mode; s the ratio; c the bow's target), the servo strip their sum against the target's.
// ?gate=bow checks the servo, the silence without breath, and the modes in tune under coupling.
import { QS, $, css, noteName, noteHz, clamp, Lab, bindPower, ensureStarted, showNote, buildKeys, markKeys, bindKeymap, fit, drawSpectrum,
  Portrait, Strip, banner, footer, navigation, offlineRun, at, gateGuard, signed } from './lab-core.js';

const KIND = 1;
const base = { voice_kind: KIND, modal_preset: 0, modes: 8, coupling: 0.05, decay_s: 3, bow_position: 0.3 };
const lab = new Lab({ params: { ...base }, traceMask: 1 << KIND, traceDecim: 2, recentPoints: 1023, snapHz: 60 });
const S = { ins: null, pts: [], breath: 0, peak: 1e-12 };
const dB = (r) => 10 * Math.log10(Math.max(r, 1e-30));
const sums = (ins) => { let e = 0, t = 0; for (let k = 0; k < ins.n; k++) { e += ins.a[k] * ins.a[k] + ins.b[k] * ins.b[k]; t += ins.c[k]; } return { e, t }; };
// THE SERVO'S BALANCE: the bow is proportional (suzu.h's bow_factor, 1 + g·(E_t − E)/E_t with g = 1/(τ·rate)), so a
// mode settles where its push equals the mode's own decay γ_k — at E/E_t = 1 − γ_k·τ, with γ_k = ln(1000)/T60 +
// β·(r_k² − 1) (SYNTH §2.5's decay law): close to the target when the decay is long and the bow quick, on it with no decay.
const balance = (ratio, p) => { const g = (p.decay_s > 0 ? Math.log(1000) / p.decay_s : 0) + (p.decay_bright ?? 0.3) * (ratio * ratio - 1); return Math.max(1e-6, 1 - g * (p.bow_onset_s ?? 0.15)); };
const predicted = (ins, p) => { let e = 0, t = 0; for (let k = 0; k < ins.n; k++) if (ins.c[k] > 0) { e += ins.c[k] * balance(ins.s[k], p); t += ins.c[k]; } return t > 0 ? e / t : 0; };

// ---- the browser check ----
function goertzel(x, f, sr) { const w = 2 * Math.PI * f / sr, c = 2 * Math.cos(w); let s1 = 0, s2 = 0; for (let i = 0; i < x.length; i++) { const s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; } return Math.sqrt(Math.max(0, s1 * s1 + s2 * s2 - c * s1 * s2)) / x.length; }
async function gate() {
  const note = 57, f0 = noteHz(note), sr = 48000;
  // (1) the bow from silence, the shipped decay: each bowed mode settles at the servo's balance; (2) the breath gone
  const script = [[0, 0xB1, 2, 80], [0, 0x91, note, 100], [at(sr, 2.5), 0xB1, 2, 0]];
  const p = { ...base, decay_bright: 0.3, bow_onset_s: 0.15 };
  const r = await offlineRun({ params: p, script, seconds: 4.5, snapHz: 20 });
  const tOf = (s) => s.block * 128 / r.sr;
  const late = r.snaps.filter((s) => s.inspect.length && tOf(s) >= 2.0 && tOf(s) <= 2.4).map((s) => s.inspect[0]);
  let worst = 0; const per = [];
  for (const ins of late) for (let k = 0; k < ins.n; k++) if (ins.c[k] > 0) {
    const meas = dB((ins.a[k] ** 2 + ins.b[k] ** 2) / ins.c[k]), pred = dB(balance(ins.s[k], p));
    worst = Math.max(worst, Math.abs(meas - pred)); if (ins === late[late.length - 1]) per.push(`${ins.s[k].toFixed(0)}: ${meas.toFixed(2)}/${pred.toFixed(2)}`);
  }
  const at24 = r.snaps.filter((s) => s.inspect.length && tOf(s) <= 2.5).pop(), at45 = r.snaps.filter((s) => s.inspect.length && tOf(s) >= 4.3).pop();
  const fell = at24 && at45 ? dB(sums(at24.inspect[0]).e / sums(at45.inspect[0]).e) : (at24 && !at45 ? Infinity : 0);
  // (3) no decay on the fundamental (T60 none) and no coupling (the lattice moves energy between modes, and the upper ones
  // keep their own decay): the bow alone moves the fundamental's energy, and holds it ON its target
  const z = await offlineRun({ params: { ...p, decay_s: 0, coupling: 0 }, script: [[0, 0xB1, 2, 80], [0, 0x91, note, 100]], seconds: 2.5, snapHz: 20 });
  const z1 = z.snaps.filter((s) => s.inspect.length && s.block * 128 / z.sr >= 2.0).map((s) => { const ins = s.inspect[0]; return dB((ins.a[0] ** 2 + ins.b[0] ** 2) / ins.c[0]); });
  const zOff = z1.length ? Math.max(...z1.map(Math.abs)) : Infinity;
  // (4) the modes in tune under coupling: the bell at κ 0.4 (its bound 0.437), struck — the first four peaks against ratio × f0
  const b = await offlineRun({ params: { ...base, modal_preset: 2, coupling: 0.4, decay_s: 8 }, script: [[0, 0x91, note, 110]], seconds: 1.2, snapHz: 10 });
  const ratios = Array.from(b.snaps.find((s) => s.inspect.length).inspect[0].s);
  const seg = b.x.subarray(Math.floor(0.2 * sr), Math.floor(1.2 * sr)), devs = [];
  for (const ratio of ratios.slice(0, 4)) { let best = 0, bf = 0; for (let c = -40; c <= 40; c += 1) { const m = goertzel(seg, ratio * f0 * Math.pow(2, c / 1200), sr); if (m > best) { best = m; bf = c; } } devs.push(bf); }
  // (5) the load gate: the bell at κ 1 is refused, with its reason
  const q = await offlineRun({ params: { ...base, modal_preset: 2, coupling: 1.0 }, script: [], seconds: 0.05 });
  return { checks: [
    { name: 'the bow\'s servo settles each mode at its balance, E/E_t = 1 − γ_k·τ (the harmonic string, A3, breath 80, T60 3 s)', pass: late.length > 4 && worst < 0.5, detail: `within ${worst.toFixed(2)} dB of the prediction, mode by mode (ratio: measured/predicted dB — ${per.join(', ')})` },
    { name: 'with no decay and no coupling the bow holds the fundamental ON its target', pass: zOff < 0.1, detail: `within ${zOff.toFixed(3)} dB from 2.0 s` },
    { name: 'no breath, no tone: the bow lets go and the modes decay', pass: fell > 20, detail: `${Number.isFinite(fell) ? fell.toFixed(1) : 'to silence'} dB down two seconds after the breath stopped` },
    { name: 'the modes stay in tune under coupling (the bell, κ 0.4 — its detune compensated at load)', pass: devs.every((d) => Math.abs(d) <= 6), detail: `the first four peaks at ${devs.map((d) => signed(d)).join(', ')} cents of their ratios (${ratios.slice(0, 4).map((x) => x.toFixed(3)).join(', ')})` },
    { name: 'the load gate refuses the bell at κ 1 and says why', pass: q.ready && !q.ready.applied && /rejected/.test(q.ready.log), detail: q.ready ? q.ready.log.trim().slice(0, 170) : 'no answer' },
  ], data: { worst, zOff, fell, devs } };
}

// ---- the live page ----
async function hold(note) { await ensureStarted(lab); lab.velocity = +$('velocity').value; lab.cc(2, S.breath); lab.hold(note); markKeys($('keys'), note, true); }
function release() { lab.release(); markKeys($('keys'), 0, false); }
const out = (el, d = 2) => { el.nextElementSibling.value = (+el.value).toFixed(d); };
async function apply(values) { const r = await lab.setParams(values); banner('refused', r.ok ? '' : `The engine refused the patch: ${r.log.trim()}`); return r; }

function drawModes() {
  const { g, w, h } = fit($('modes'));
  g.clearRect(0, 0, w, h);
  const ins = S.ins;
  if (!ins || !ins.n) { g.fillStyle = css('--muted'); g.font = '12px system-ui, sans-serif'; g.textAlign = 'center'; g.fillText(lab.ctx ? 'Hold a note — the modes appear when it sounds.' : 'Start the sound, then hold a note.', w / 2, h / 2); return; }
  const n = ins.n; let rmax = 1; for (let k = 0; k < n; k++) rmax = Math.max(rmax, ins.s[k]);
  let rmin = 1; for (let k = 0; k < n; k++) rmin = Math.min(rmin, ins.s[k]);
  const lo = rmin * 0.85, hi = rmax * 1.15, X = (r) => 30 + (w - 50) * Math.log(r / lo) / Math.log(hi / lo);
  const E = Array.from({ length: n }, (_, k) => ins.a[k] * ins.a[k] + ins.b[k] * ins.b[k]);
  const ref = Math.max(...E, ...Array.from(ins.c.subarray(0, n)), 1e-12); S.peak = Math.max(ref, S.peak * 0.995);
  const top = 26, dbLo = -60, Y = (e) => (h - 22) - (h - 22 - top) * (clamp(dB(e / S.peak), dbLo, 0) - dbLo) / -dbLo;   // the top row is the captions'
  g.strokeStyle = css('--rule'); g.lineWidth = 1; g.font = '10px system-ui, sans-serif'; g.fillStyle = css('--muted'); g.textAlign = 'right';
  for (const d of [0, -20, -40, -60]) { const y = (h - 22) - (h - 22 - top) * (d - dbLo) / -dbLo; g.beginPath(); g.moveTo(30, y); g.lineTo(w - 10, y); g.stroke(); g.fillText(`${d}`, 26, y + 3); }
  const bw = Math.max(4, Math.min(18, (w - 60) / (n * 3)));
  g.textAlign = 'center';
  for (let k = 0; k < n; k++) {
    const x = X(ins.s[k]);
    g.fillStyle = css('--p'); g.fillRect(x - bw / 2, Y(E[k]), bw, (h - 22) - Y(E[k]));
    if (ins.c[k] > 0) {
      g.strokeStyle = css('--u'); g.lineWidth = 2; const y = Y(ins.c[k]); g.beginPath(); g.moveTo(x - bw, y); g.lineTo(x + bw, y); g.stroke();
      g.setLineDash([2, 2]); const yb = Y(ins.c[k] * balance(ins.s[k], lab.params)); g.beginPath(); g.moveTo(x - bw, yb); g.lineTo(x + bw, yb); g.stroke(); g.setLineDash([]);
    }
  }
  // the ratios under the bars, left to right, a label left out where it would overlap its neighbour (a phone, the bell's 2.50 2.67 3.00)
  g.fillStyle = css('--muted'); let right = -Infinity;
  for (const k of Array.from({ length: n }, (_, k) => k).sort((a, b) => ins.s[a] - ins.s[b])) {
    const t = ins.s[k] < 10 ? ins.s[k].toFixed(2) : ins.s[k].toFixed(1), x = X(ins.s[k]), half = g.measureText(t).width / 2;
    if (x - half < right + 4) continue;
    g.fillText(t, x, h - 6); right = x + half;
  }
  g.textAlign = 'left'; g.fillText('dB from the loudest', 32, 12);
  const cap = 'the mode\'s frequency over the note\'s';
  g.textAlign = 'right'; g.fillText(32 + g.measureText('dB from the loudest').width + 16 + g.measureText(cap).width < w - 10 ? cap : 'over the note', w - 10, 12);
}

function wire() {
  navigation('modal');
  const orbit = new Portrait($('orbit')), servo = new Strip($('servo'), 12);
  lab.on('ready', () => { $('engine-line').textContent = `Voxo ${lab.version}, the modal lattice (voice kind ${KIND}), ${lab.sampleRate} Hz`; if (QS.has('demo')) { $('preset').value = '2'; apply({ modal_preset: 2 }).then(() => { S.breath = 60; $('breath').value = 60; out($('breath'), 0); hold(57); }); } });
  lab.on('snap', (m) => {
    S.ins = m.inspect.find((v) => v.held) || m.inspect[m.inspect.length - 1] || null;
    if (m.recent && m.recent.length >= 8 && S.ins) { const r = m.recent, pts = []; for (let i = 0; i < r.length; i += 4) pts.push([r[i], r[i + 1]]); S.pts = pts.slice(-Math.round(2 * S.ins.rate2 / 2 / Math.max(S.ins.freq, 1))); }
    else if (!S.ins) S.pts = [];
  });
  buildKeys($('keys'), { lo: 36, hi: 84, onDown: (m) => hold(m) });
  bindKeymap(60, (m) => hold(m));
  bindPower(lab, $('power'));
  $('volume').addEventListener('input', (e) => lab.gain(+e.target.value));
  for (const id of ['breath', 'velocity', 'modes-n']) out($(id), 0);
  for (const id of ['coupling', 'decay', 'bowpos']) out($(id));
  $('preset').addEventListener('change', (e) => apply({ modal_preset: +e.target.value }).then(() => { if (lab.held) hold(lab.note); }));
  $('breath').addEventListener('input', (e) => { out(e.target, 0); S.breath = +e.target.value; lab.cc(2, S.breath); });
  $('coupling').addEventListener('input', (e) => { out(e.target); apply({ coupling: +e.target.value }); });
  $('modes-n').addEventListener('input', (e) => { out(e.target, 0); apply({ modes: +e.target.value }); });
  $('decay').addEventListener('input', (e) => { out(e.target); apply({ decay_s: +e.target.value }); });
  $('bowpos').addEventListener('input', (e) => { out(e.target); apply({ bow_position: +e.target.value }); });
  $('velocity').addEventListener('input', (e) => { out(e.target, 0); lab.velocity = +e.target.value; });
  $('strike').addEventListener('click', () => hold(lab.note === 60 && !lab.held ? 57 : lab.note));
  $('stop').addEventListener('click', release);
  const tick = () => {
    lab.readAudio();
    const f0 = noteHz(lab.note), ins = S.ins;
    drawModes();
    orbit.draw([S.pts], { fade: 0.14, xLabel: 'Σx', yLabel: 'Σy' });
    drawSpectrum($('spectrum'), lab, { f0, ratios: ins ? Array.from(ins.s.subarray(0, ins.n)) : null, sounding: lab.pitch() });
    const sm = ins ? sums(ins) : null;
    servo.push({ e: sm ? dB(sm.e) : null, tgt: sm && sm.t > 0 ? dB(sm.t) : null });
    servo.draw([{ key: 'tgt', color: css('--u'), lo: -90, hi: 0, width: 1.5 }, { key: 'e', color: css('--trace'), lo: -90, hi: 0, width: 2 }], [[-20, '−20 dB', 'e'], [-50, '−50 dB', 'e'], [-80, '−80 dB', 'e']]);
    $('r-note').textContent = `${noteName(lab.note)} · ${f0.toFixed(1)} Hz`;
    $('r-modes').textContent = ins ? `${ins.n}, κ ${ins.k[0].toFixed(2)}` : '—';
    const pb = ins && sm && sm.t > 0 ? predicted(ins, lab.params) : 0;
    $('r-servo').textContent = sm && sm.t > 0 ? `${signed(dB(sm.e / sm.t), 2)} dB` : (S.breath > 0 ? '—' : 'no bow');
    $('r-balance').textContent = sm && sm.t > 0 ? `${signed(dB(pb), 2)} dB` : '—';
    footer(lab, 'modal lattice');
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
  if (QS.has('demo')) ensureStarted(lab);
}

if (QS.get('gate') === 'bow') gateGuard('modal', gate); else wire();
