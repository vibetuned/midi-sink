// strings.js — the Suzu lab's strings (Phase 8 step 59c, DECISIONS_7 #31; SYNTH §2.5, §2.8, §2.9).
//
// Voice kinds 2 (the Verlet chain), 3 (the hybrid: a delay line closed by a bridge with modes) and 1 with
// the plucked-string preset (Karplus–Strong in modal form). ?gate=strings checks each on its note and the
// chain's CFL red control: refused with the gate, blown up (and caught) without it.
import { QS, $, css, noteName, noteHz, clamp, Lab, bindPower, ensureStarted, showNote, buildKeys, markKeys, bindKeymap, fit, drawSpectrum,
  Portrait, banner, footer, navigation, offlineRun, at, pitchAt, gateGuard, register, signed, peakNear } from './lab-core.js';

const PATCH = {
  2: { voice_kind: 2, string_nodes: 48, string_decay_s: 4, pluck: 0.28, pickup: 0.25, cfl_gate: 1, string_cfl: 0 },
  3: { voice_kind: 3, string_decay_s: 4, pluck: 0.28, pickup: 0.25, bridge_coupling: 0.002, bridge_cells: 2 },
  1: { voice_kind: 1, modal_preset: 4, modes: 16, decay_s: 4, pluck: 0.28 },
};
const NOTES = {
  2: 'Forty-eight masses on springs, stepped by Verlet: the displacement along the string, its ends fixed. The pluck starts it as a triangle that splits into two waves running to the ends and back; where the pickup listens, the sound is the motion there.',
  3: 'A delay line closed by a bridge: the loop\'s wave going round (the circle, read from the oldest sample to the newest), and the bridge\'s modes — little cells of their own the string drives through the junction — on the right. The body speaks: its modes ring at their own frequencies beside the string\'s, as loud as it in the first second, so the readout follows the spectral peak at the note.',
  1: 'Sixteen modes, each a cell, struck by a pluck\'s profile: the string you see is their sum, Σ x_k·sin(kπs). The highs decay first, as a real string\'s do, and the triangle softens into a sine.',
};
const KLABEL = { 2: 'k·dt² (the CFL number, ≤ 1)', 3: 'The loop, sub-steps', 1: 'Modes' };
const lab = new Lab({ params: { ...PATCH[2] }, traceMask: 0x1ff, traceDecim: 2, recentPoints: 1023, snapHz: 60 });
const S = { kind: 2, ins: null, pts: [], scale: 1e-6, hz: 0 };

// ---- the browser check ----
async function gate() {
  const note = 57, f0 = noteHz(note), sr = 48000, checks = [], rows = [];
  for (const k of [2, 1]) {
    const r = await offlineRun({ params: { ...PATCH[k] }, script: [[0, 0x91, note, 100]], seconds: 1.2 });
    const cs = [0.4, 0.6, 0.8, 1.0].map((t) => { const reg = register(pitchAt(r.x, sr, t), f0); return reg && reg.reg === 1 ? reg.cents : NaN; });
    const med = cs.filter(Number.isFinite).sort((a, b) => a - b)[1] ?? NaN;
    rows.push({ kind: k, cents: cs });
    checks.push({ name: `${k === 2 ? 'the Verlet chain' : 'the modal pluck'} sounds its note (A3, plucked)`, pass: Number.isFinite(med) && Math.abs(med) <= 20, detail: `cents at 0.4, 0.6, 0.8, 1.0 s: ${cs.map((c) => Number.isFinite(c) ? signed(c, 1) : '—').join(', ')}` });
  }
  {   // the hybrid: its body's modes ring at their own frequencies beside the string (DECISIONS_7 #14), so the waveform's
      // period is theirs and the string's together — the string's fundamental is the spectral peak at the note
    const r = await offlineRun({ params: { ...PATCH[3] }, script: [[0, 0x91, note, 100]], seconds: 1.2 });
    const seg = r.x.subarray(Math.floor(0.2 * sr), Math.floor(1.2 * sr)), pk = peakNear(seg, sr, f0, 60), c = 1200 * Math.log2(pk.hz / f0);
    const body = [341, 472].map((f) => peakNear(seg, sr, f, 80));
    checks.push({ name: 'the hybrid\'s string sounds its note (A3, plucked; the peak at the note)', pass: Math.abs(c) <= 4, detail: `${signed(c, 1)} cents; its body rings beside it at ${body.map((b) => `${b.hz.toFixed(0)} Hz (${(20 * Math.log10(b.amp / pk.amp)).toFixed(0)} dB)`).join(' and ')} — the bridge's modes, pulled by the string` });
  }
  const refused = await offlineRun({ params: { ...PATCH[2], string_cfl: 1.05 }, script: [], seconds: 0.05 });
  checks.push({ name: 'the CFL gate refuses k·dt² = 1.05 and says why', pass: refused.ready && !refused.ready.applied && /rejected/.test(refused.ready.log), detail: refused.ready ? refused.ready.log.trim().slice(0, 170) : 'no answer' });
  const blown = await offlineRun({ params: { ...PATCH[2], string_cfl: 1.05, cfl_gate: 0 }, script: [[0, 0x91, note, 100]], seconds: 2.0 });
  checks.push({ name: 'with the gate bypassed the chain blows up — and the lab catches it (the red control)', pass: blown.ready?.applied && blown.blowups.length > 0, detail: blown.blowups.length ? `caught at ${(blown.blowups[0].block * 128 / sr).toFixed(3)} s; the engine restarted ${blown.restarts.length}×` : 'not caught' });
  return { checks, data: { rows } };
}

// ---- the live page ----
async function hold(note) { await ensureStarted(lab); lab.velocity = +$('velocity').value; lab.hold(note); markKeys($('keys'), note, true); }
function release() { lab.release(); markKeys($('keys'), 0, false); }
const out = (el, d = 2) => { el.nextElementSibling.value = (+el.value).toFixed(d); };
async function apply(values) { const r = await lab.setParams(values); if (!r.ok) banner('refused', `The engine refused the patch: ${r.log.trim()}`); else if (!$('cflgate').checked) banner(); return r; }
const PLUCK_PRESETS = {
  default: { modal_preset: 4, modes: 16, decay_s: 4, pluck: 0.28, stiffness: 0, decay_bright: 0.3, hint: 'Sixteen modes, each a cell, struck by a pluck\'s profile: the string you see is their sum, Σ x_k·sin(kπs). The highs decay first, as a real string\'s do, and the triangle softens into a sine.' },
  vcsl_dan_tranh: { modal_preset: 0, modes: 13, decay_s: 3.435, pluck: 0.3, stiffness: 0.000132, decay_bright: 0.0836, hint: 'Dan Tranh (VCSL fitted) — Vietnamese 16-string plucked zither: 13 modes, steel wire stiffness B = 1.32e-4, T60 = 3.44 s, pluck 0.30.' },
  vcsl_concert_harp: { modal_preset: 4, modes: 5, decay_s: 5.728, pluck: 0.445, stiffness: 0.000206, decay_bright: 0.0088, hint: 'Concert Harp (VCSL fitted) — pedal harp string pluck: 5 modes, T60 = 5.73 s, pluck position 0.445.' },
};

async function setKind(k) {
  S.kind = k; S.scale = 1e-6;
  for (const b of $('kind').querySelectorAll('button')) b.classList.toggle('on', +b.dataset.k === k);
  for (const el of document.querySelectorAll('.only-1')) el.hidden = k !== 1;
  for (const el of document.querySelectorAll('.only-2')) el.hidden = k !== 2;
  for (const el of document.querySelectorAll('.only-3')) el.hidden = k !== 3;
  const pkKey = ($('pluck-preset') && $('pluck-preset').value) || 'default';
  const pkCfg = PLUCK_PRESETS[pkKey] || PLUCK_PRESETS.default;
  $('string-note').textContent = k === 1 ? pkCfg.hint : NOTES[k];
  $('r-k-label').textContent = KLABEL[k];
  $('phase-legend').textContent = k === 1 ? 'the lattice (Σx, Σy)' : 'the sound against its slope (s, ṡ/ω)';
  const p = { ...PATCH[k], pluck: +$('pluck').value, pickup: +$('pickup').value };
  if (k === 1) {
    Object.assign(p, pkCfg);
    p.pluck = +$('pluck').value;
    p.decay_s = +$('decay').value;
    delete p.hint;
  } else p.string_decay_s = +$('decay').value;
  if (k === 2) p.string_nodes = +$('nodes').value;
  if (k === 3) { p.bridge_coupling = +$('bridge').value; p.bridge_cells = +$('bcells').value; }
  if (k === 2 && $('cfl').checked) { p.string_cfl = 1.05; p.cfl_gate = $('cflgate').checked ? 0 : 1; }
  const r = await apply(p);
  if (r.ok && lab.held) hold(lab.note);
}

function drawString() {
  const { g, w, h } = fit($('string'));
  g.clearRect(0, 0, w, h);
  const ins = S.ins;
  g.fillStyle = css('--muted'); g.font = '11px system-ui, sans-serif'; g.textAlign = 'center';
  if (!ins || !ins.n) { g.fillText(lab.ctx ? 'Pluck a note — the string appears while it sounds.' : 'Start the sound, then pluck a note.', w / 2, h / 2); return; }
  const x0 = 24, x1 = w - 24, yc = h / 2, A = h * 0.38;
  const mark = (s, label, row = 0) => { const x = x0 + (x1 - x0) * s; g.strokeStyle = css('--rule'); g.setLineDash([3, 4]); g.beginPath(); g.moveTo(x, 14); g.lineTo(x, h - 16 - 12 * row); g.stroke(); g.setLineDash([]); g.fillText(label, x, h - 4 - 12 * row); };
  if (S.kind === 2 || S.kind === 1) {
    const N = S.kind === 2 ? ins.n : 128, ys = new Float32Array(N);
    if (S.kind === 2) for (let i = 0; i < N; i++) ys[i] = ins.a[i];
    else for (let i = 0; i < N; i++) { const s = i / (N - 1); let u = 0; for (let k = 0; k < ins.n; k++) u += ins.a[k] * Math.sin((k + 1) * Math.PI * s); ys[i] = u; }
    let pk = 0; for (const v of ys) pk = Math.max(pk, Math.abs(v)); S.scale = Math.max(pk, S.scale * 0.985, 1e-9);
    mark(+$('pluck').value, 'pluck'); if (S.kind === 2) mark(+$('pickup').value, 'pickup', 1);
    g.strokeStyle = css('--rule'); g.beginPath(); g.moveTo(x0, yc); g.lineTo(x1, yc); g.stroke();
    g.strokeStyle = css('--p'); g.lineWidth = 2.2; g.beginPath();
    for (let i = 0; i < N; i++) g.lineTo(x0 + (x1 - x0) * i / (N - 1), yc - clamp(ys[i] / S.scale, -1, 1) * A);
    g.stroke();
    if (S.kind === 2) { g.fillStyle = css('--p'); for (let i = 0; i < N; i++) g.fillRect(x0 + (x1 - x0) * i / (N - 1) - 1.5, yc - clamp(ys[i] / S.scale, -1, 1) * A - 1.5, 3, 3); }
    g.fillStyle = css('--tube'); g.fillRect(x0 - 6, yc - 18, 4, 36); g.fillRect(x1 + 2, yc - 18, 4, 36);
    return;
  }
  // the hybrid: the loop as a circle, the bridge's cells as dials
  const n = ins.n, cx = Math.min(w * 0.35, h * 0.9), cy = yc, R = Math.min(h * 0.32, w * 0.2);
  let pk = 0; for (let i = 0; i < n; i++) pk = Math.max(pk, Math.abs(ins.a[i])); S.scale = Math.max(pk, S.scale * 0.985, 1e-9);
  g.strokeStyle = css('--rule'); g.beginPath(); g.arc(cx, cy, R, 0, 2 * Math.PI); g.stroke();
  g.strokeStyle = css('--p'); g.lineWidth = 2; g.beginPath();
  for (let i = 0; i <= n; i++) { const j = i % n, th = -Math.PI / 2 + 2 * Math.PI * i / n, r = R * (1 + 0.45 * clamp(ins.a[j] / S.scale, -1, 1)); g.lineTo(cx + r * Math.cos(th), cy + r * Math.sin(th)); }
  g.stroke();
  g.fillStyle = css('--muted'); g.fillText(`the loop: ${ins.k[6].toFixed(0)} sub-steps a turn`, cx, cy + R * 1.55 + 4);
  const nb = Math.round(+$('bcells').value), dx = (w - (cx + R * 1.7)) / Math.max(nb, 1);
  for (let b = 0; b < nb; b++) {
    const bx = cx + R * 1.7 + dx * (b + 0.5), r = Math.min(dx * 0.35, h * 0.28), x = ins.k[b], y = ins.k[3 + b], m = Math.hypot(x, y);
    S[`b${b}`] = Math.max(m, (S[`b${b}`] || 0) * 0.985, 1e-12);
    g.strokeStyle = css('--rule'); g.beginPath(); g.arc(bx, cy, r, 0, 2 * Math.PI); g.stroke();
    g.fillStyle = css('--u'); g.beginPath(); g.arc(bx + r * x / S[`b${b}`], cy - r * y / S[`b${b}`], 4, 0, 2 * Math.PI); g.fill();
    g.fillStyle = css('--muted'); g.fillText(`bridge mode ${b + 1}`, bx, cy + r + 16);
  }
}

function wire() {
  navigation('strings');
  const phase = new Portrait($('phase'));
  lab.on('ready', () => { $('engine-line').textContent = `Voxo ${lab.version}, the strings, ${lab.sampleRate} Hz`; if (QS.has('demo')) hold(45); });
  lab.on('snap', (m) => {
    S.ins = m.inspect.find((v) => v.held) || m.inspect[m.inspect.length - 1] || null;
    if (m.recent && m.recent.length >= 8 && S.ins) { const r = m.recent, pts = []; for (let i = 0; i < r.length; i += 4) pts.push([r[i], r[i + 1]]); S.pts = pts.slice(-Math.round(2 * S.ins.rate2 / 2 / Math.max(S.ins.freq, 1))); }
    else if (!S.ins) S.pts = [];
  });
  lab.on('blowup', (m) => banner('red', `It blew up ${(m.block * 128 / lab.sampleRate).toFixed(2)} s into the engine's life: past k·dt² = 1 the chain's shortest wave is amplified every step instead of carried, so it grew until its samples were no longer numbers. The lab muted it and restarted the engine — this is the reason the CFL gate refuses the patch.`));
  buildKeys($('keys'), { lo: 36, hi: 84, onDown: (m) => hold(m) });
  bindKeymap(48, (m) => hold(m));
  bindPower(lab, $('power'));
  $('volume').addEventListener('input', (e) => lab.gain(+e.target.value));
  for (const id of ['pluck', 'pickup', 'decay']) out($(id));
  for (const id of ['velocity', 'nodes', 'bcells']) out($(id), 0);
  out($('bridge'), 4);
  for (const b of $('kind').querySelectorAll('button')) b.addEventListener('click', () => setKind(+b.dataset.k));
  $('pluck').addEventListener('input', (e) => { out(e.target); apply({ pluck: +e.target.value }); });
  $('pickup').addEventListener('input', (e) => { out(e.target); apply({ pickup: +e.target.value }); });
  $('decay').addEventListener('input', (e) => { out(e.target); apply(S.kind === 1 ? { decay_s: +e.target.value } : { string_decay_s: +e.target.value }); });
  $('nodes').addEventListener('input', (e) => { out(e.target, 0); apply({ string_nodes: +e.target.value }); });
  $('bridge').addEventListener('input', (e) => { out(e.target, 4); apply({ bridge_coupling: +e.target.value }); });
  $('bcells').addEventListener('input', (e) => { out(e.target, 0); apply({ bridge_cells: +e.target.value }); });
  $('velocity').addEventListener('input', (e) => { out(e.target, 0); lab.velocity = +e.target.value; });
  $('cfl').addEventListener('change', () => setKind(2));
  $('cflgate').addEventListener('change', () => { setKind(2); if ($('cflgate').checked) banner('red', 'The CFL gate is bypassed. With k·dt² forced over 1, pluck a note and watch the chain.'); });
  $('pluck-preset').addEventListener('change', () => {
    const pkKey = $('pluck-preset').value;
    const pkCfg = PLUCK_PRESETS[pkKey];
    if (pkCfg) {
      if (pkCfg.pluck !== undefined) { $('pluck').value = pkCfg.pluck; out($('pluck')); }
      if (pkCfg.decay_s !== undefined) { $('decay').value = pkCfg.decay_s; out($('decay')); }
      setKind(1);
    }
  });
  $('again').addEventListener('click', () => hold(lab.held || lab.note !== 60 ? lab.note : 45));
  $('stop').addEventListener('click', release);
  if (QS.has('preset')) {
    const qp = QS.get('preset');
    if (qp === 'dan_tranh' || qp === 'vcsl_dan_tranh') {
      $('pluck-preset').value = 'vcsl_dan_tranh';
      const pkCfg = PLUCK_PRESETS.vcsl_dan_tranh;
      $('pluck').value = pkCfg.pluck; out($('pluck'));
      $('decay').value = pkCfg.decay_s; out($('decay'));
      setKind(1);
    } else if (qp === 'concert_harp' || qp === 'vcsl_concert_harp') {
      $('pluck-preset').value = 'vcsl_concert_harp';
      const pkCfg = PLUCK_PRESETS.vcsl_concert_harp;
      $('pluck').value = pkCfg.pluck; out($('pluck'));
      $('decay').value = pkCfg.decay_s; out($('decay'));
      setKind(1);
    } else {
      setKind(2);
    }
  } else {
    setKind(2);
  }
  const tick = () => {
    lab.readAudio();
    S.hz = S.kind === 3 && lab.held && lab.timeBuf ? peakNear(lab.timeBuf, lab.sampleRate, noteHz(lab.note), 60).hz : lab.pitch();   // the hybrid: the string's peak, not the body's period
    const f0 = noteHz(lab.note), ins = S.ins, reg = register(S.hz, f0);
    drawString();
    phase.draw([S.pts], { fade: 0.14, xLabel: S.kind === 1 ? 'Σx' : 's', yLabel: S.kind === 1 ? 'Σy' : 'ṡ / ω' });
    drawSpectrum($('spectrum'), lab, { f0, sounding: S.hz });
    $('r-note').textContent = `${noteName(lab.note)} · ${f0.toFixed(1)} Hz`;
    $('r-hz').textContent = reg ? `${S.hz.toFixed(1)} Hz` : (lab.held ? 'silent' : '—');
    $('r-cents').textContent = reg ? `${signed(reg.cents)} cents` : '—';
    $('r-k').textContent = !ins ? '—' : S.kind === 2 ? ins.k[0].toFixed(4) : S.kind === 3 ? ins.k[6].toFixed(0) : String(ins.n);
    footer(lab, S.kind === 2 ? 'Verlet chain' : S.kind === 3 ? 'hybrid string' : 'modal pluck');
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
  if (QS.has('demo')) ensureStarted(lab);
}

if (QS.get('gate') === 'strings') gateGuard('strings', gate); else wire();
