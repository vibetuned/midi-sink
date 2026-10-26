// toda.js — the Suzu lab's Toda ring (v3.0 step 92; the voice of step 83, DECISIONS_13 #1; TO_PROJECT_SPEC §13.13).
//
// Voice kind 10: N nodes on a ring under the exponential bond, the kick–drift leapfrog with the state in double and
// the ring's own exponential; the press is the nonlinearity b. The resolution rule holds √k·ĉ·b under 0.15 by two to
// eight inner steps a sub-step and caps b for the note where eight cannot; the pickup stands still against the mean
// drift. ?gate=toda checks the linear ring on the note, the pressed strike's soliton train sharp of it, the energy
// held lossless, and the red control — the gate off — making energy by the tens of decibels a second at C8.
import { QS, $, css, noteName, noteHz, clamp, Lab, bindPower, ensureStarted, buildKeys, markKeys, bindKeymap, drawSpectrum,
  Portrait, Strip, banner, footer, navigation, offlineRun, gateGuard, register, signed, peakNear, fit, DOCS_ROOT } from './lab-core.js';

const KIND = 10;
// the patch table's Toda ring (voxo/src/suzu_patches.cpp)
const BASE = { voice_kind: KIND, toda_nodes: 16, toda_b: 0.6, toda_cap: 6, toda_width: 1.5, toda_gate: 1, decay_s: 3, pickup: 0.3,
  level: 0.25, attack_s: 0.002, release_s: 0.5, cutoff_hz: 20000, resonance: 0 };
const lab = new Lab({ params: { ...BASE }, traceMask: 1 << KIND, traceDecim: 4, recentPoints: 256, snapHz: 30, strobe: 1 });
const S = { ins: null, dots: [], serial: -1, h0: 0, hNow: 0, rScale: 1e-6, hz: 0, press: 0 };
const dB = (r) => 10 * Math.log10(Math.max(r, 1e-30));
const mean = (a) => a.reduce((s, v) => s + v, 0) / Math.max(a.length, 1);

// ---- the browser check ----
async function gate() {
  const sr = 48000, note = 57, f0 = noteHz(note);
  const cents = (hz) => 1200 * Math.log2(hz / f0);
  // (1) softly struck, no press: the linear modes, the first on the note — the suite's measure (gate 49): the spectral
  // peak within ±100 cents of the note over the second from 0.5 s (the ring's modes are the lattice's sin(mπ/N), not
  // harmonics, so a period detector reads a beat — the line is the measure)
  const seg = (r) => r.x.subarray(Math.floor(0.5 * sr), Math.floor(1.5 * sr));
  const soft = await offlineRun({ params: { ...BASE, decay_s: 0 }, script: [[0, 0x91, note, 25]], seconds: 1.5 });
  const softC = cents(peakNear(seg(soft), sr, f0, 100).hz);
  // (2) struck hard under full press: a train of solitons passing the pickup faster than sound — the fundamental
  // hundreds of cents above the note (the suite: +300 at least, the peak within ±700 cents over the same second)
  const hard = await offlineRun({ params: { ...BASE, decay_s: 0 }, script: [[0, 0xD1, 127, 0], [0, 0x91, note, 127]], seconds: 1.5, snapHz: 20 });
  const hardC = cents(peakNear(seg(hard), sr, f0, 700).hz);
  const hk = hard.snaps.find((s) => s.inspect.length && s.block * 128 / sr > 0.1)?.inspect[0].k ?? null;
  // (3) lossless under a press: H (the capped potential's) holds — the second-mean, since the instantaneous breathes
  const loss = await offlineRun({ params: { ...BASE, decay_s: 0 }, script: [[0, 0xD1, 100, 0], [0, 0x91, note, 110]], seconds: 4.0, snapHz: 30 });
  const H = (t0, t1) => mean(loss.snaps.filter((s) => s.inspect.length && s.block * 128 / sr >= t0 && s.block * 128 / sr < t1).map((s) => s.inspect[0].k[2]));
  const drift = dB(H(2.5, 3.5) / H(0.5, 1.5));
  // (4) the red: the gate off at C8 under full press — the ring as written, two inner steps, the cap and b as asked
  const red = await offlineRun({ params: { ...BASE, decay_s: 0, toda_gate: 0 }, script: [[0, 0xD1, 127, 0], [0, 0x91, 108, 127]], seconds: 1.5, snapHz: 30 });
  const rh = red.snaps.filter((s) => s.inspect.length).map((s) => [s.block * 128 / sr, s.inspect[0].k[2]]);
  const rh0 = rh.find(([t]) => t >= 0.1)?.[1] ?? 0, rhMax = Math.max(0, ...rh.filter(([t]) => t >= 0.1 && t <= 1.2).map(([, h]) => h));
  const climb = rh0 > 0 ? dB(rhMax / rh0) : (red.blowups.length ? Infinity : 0);
  return { checks: [
    { name: 'softly struck with no press the ring is linear: its first mode sounds the note (A3, velocity 25; the line over the second from 0.5 s)', pass: Number.isFinite(softC) && Math.abs(softC) < 10, detail: `${signed(softC, 1)} cents` },
    { name: 'struck hard under full press the pulse is a train of solitons passing the pickup faster than sound — the fundamental hundreds of cents above the note (velocity 127, b 0.6 × the press)', pass: Number.isFinite(hardC) && hardC > 300, detail: `${signed(hardC)} cents over the second from 0.5 s${hk ? ` — the rule ran ${hk[6]} inner steps, b ${hk[1].toFixed(2)} of ${hk[7].toFixed(2)} asked, the ceiling ${hk[8].toFixed(2)}` : ''}` },
    { name: 'lossless, the ring holds its energy: the second-mean of H (the instantaneous breathes ±0.06 dB)', pass: Math.abs(drift) < 0.05, detail: `${signed(drift, 4)} dB between the first and the third second` },
    { name: 'the red control: the Toda gate off at C8 under full press — the integrator makes energy (the suite: +19 dB in a second)', pass: climb > 6 || red.blowups.length > 0, detail: red.blowups.length ? `blew up at ${(red.blowups[0].block * 128 / sr).toFixed(2)} s and the lab caught it` : `H ${signed(climb, 1)} dB within the second` },
  ], data: { softC, hardC, drift, climb } };
}

// ---- the live page ----
async function hold(note, velocity) { await ensureStarted(lab); lab.velocity = velocity ?? +$('velocity').value; lab.pressure(S.press); lab.hold(note); markKeys($('keys'), note, true); }
function release() { lab.release(); markKeys($('keys'), 0, false); }
const out = (el, d = 2) => { el.nextElementSibling.value = (+el.value).toFixed(d); };
async function apply(values) { const r = await lab.setParams(values); if (!r.ok) banner('refused', `The engine refused the patch: ${r.log.trim()}`); else if (!$('red').checked) banner(); return r; }

function drawRing() {
  const { g, w, h } = fit($('ring'));
  g.clearRect(0, 0, w, h);
  const ins = S.ins;
  if (!ins || !ins.n) { g.fillStyle = css('--muted'); g.font = '12px system-ui, sans-serif'; g.textAlign = 'center'; g.fillText(lab.ctx ? 'Strike a key — the ring appears when it sounds.' : 'Start the sound, then strike a key.', w / 2, h / 2); return; }
  const n = ins.n, cx = w / 2, cy = h / 2, R = Math.min(w, h) * 0.36;
  const r = new Float32Array(n); let peak = 1e-9;
  for (let i = 0; i < n; i++) { r[i] = ins.a[(i + 1) % n] - ins.a[i]; peak = Math.max(peak, Math.abs(r[i])); }
  S.rScale = Math.max(peak, S.rScale * 0.96);
  g.strokeStyle = css('--rule'); g.lineWidth = 1; g.beginPath(); g.arc(cx, cy, R, 0, 2 * Math.PI); g.stroke();
  g.strokeStyle = css('--trace'); g.lineWidth = 1.5; g.beginPath();
  for (let i = 0; i <= n; i++) { const k = i % n, th = 2 * Math.PI * k / n, rr = R * (1 - 0.35 * clamp(-r[k] / S.rScale, -1, 1)); const x = cx + rr * Math.cos(th), y = cy + rr * Math.sin(th); if (i === 0) g.moveTo(x, y); else g.lineTo(x, y); }
  g.stroke();
  for (let i = 0; i < n; i++) {
    const th = 2 * Math.PI * i / n, rr = R * (1 - 0.35 * clamp(-r[i] / S.rScale, -1, 1)), c = clamp(-r[i] / S.rScale, 0, 1);
    g.fillStyle = css('--trace'); g.globalAlpha = 0.25 + 0.75 * c; g.beginPath(); g.arc(cx + rr * Math.cos(th), cy + rr * Math.sin(th), 3 + 4 * c, 0, 2 * Math.PI); g.fill();
  }
  g.globalAlpha = 1;
  g.fillStyle = css('--muted'); g.font = '11px system-ui, sans-serif'; g.textAlign = 'center'; g.fillText(`${n} nodes · compression lights a node and pulls it in`, cx, h - 6);
}

function wire() {
  navigation('toda'); $('chapter-link').href = `${DOCS_ROOT}suzu/toda-ring/`;
  const strobe = new Portrait($('strobe')), hist = new Strip($('history'), 6);
  lab.on('ready', () => { $('engine-line').textContent = `Voxo ${lab.version}, the Toda ring (voice kind ${KIND}), ${lab.sampleRate} Hz`; if (QS.has('demo')) { $('press').value = 110; out($('press'), 0); S.press = 110; hold(57, 127); } });
  lab.on('snap', (m) => {
    S.ins = m.inspect.find((v) => v.held) || m.inspect[m.inspect.length - 1] || null;
    if (S.ins) { if (S.ins.serial !== S.serial) { S.serial = S.ins.serial; S.h0 = S.ins.k[2]; } S.hNow = S.ins.k[2]; }
    if (m.strobe) for (const r of m.strobe) for (let i = 0; i < r.count; i++) S.dots.push([r.x[i], r.y[i]]);
  });
  lab.on('blowup', (m) => { banner('red', `It blew up ${(m.block * 128 / lab.sampleRate).toFixed(2)} s into the engine's life: with the gate off a step's displacement outran the exponential's length and the leapfrog made energy until its samples were no longer numbers. The lab muted it and restarted the engine — switch the red control off to play again.`); });
  buildKeys($('keys'), { lo: 36, hi: 96, onDown: (m) => hold(m) });
  bindKeymap(57, (m) => hold(m));
  bindPower(lab, $('power'));
  $('volume').addEventListener('input', (e) => lab.gain(+e.target.value));
  for (const id of ['b', 'width', 'cap', 'decay']) out($(id));
  for (const id of ['press', 'nodes', 'velocity']) out($(id), 0);
  $('press').addEventListener('input', (e) => { out(e.target, 0); S.press = +e.target.value; lab.pressure(S.press); });
  $('b').addEventListener('input', (e) => { out(e.target); apply({ toda_b: +e.target.value }); });
  $('nodes').addEventListener('input', (e) => { out(e.target, 0); apply({ toda_nodes: +e.target.value }); });
  $('width').addEventListener('input', (e) => { out(e.target); apply({ toda_width: +e.target.value }); });
  $('cap').addEventListener('input', (e) => { out(e.target); apply({ toda_cap: +e.target.value }); });
  $('decay').addEventListener('input', (e) => { out(e.target); apply({ decay_s: +e.target.value }); });
  $('velocity').addEventListener('input', (e) => { out(e.target, 0); lab.velocity = +e.target.value; });
  $('hard').addEventListener('click', () => { $('press').value = 127; out($('press'), 0); S.press = 127; hold(lab.held ? lab.note : 57, 127); });
  $('stop').addEventListener('click', release);
  $('red').addEventListener('change', async (e) => {
    await apply({ toda_gate: e.target.checked ? 0 : 1 });
    if (e.target.checked) banner('red', 'The Toda gate is off: two inner steps, the cap and b as asked. Press hard and strike high — at C8 the suite measures +19 dB of energy in a second, and the lab catches the blow-up when the samples stop being numbers.'); else banner();
  });
  const tick = () => {
    lab.readAudio(); S.hz = lab.pitch();
    const f0 = noteHz(lab.note), reg = register(S.hz, f0), ins = S.ins;
    drawRing();
    strobe.draw([S.dots.splice(0)], { fade: 0.01, dots: true, xLabel: 'x', yLabel: 'y' });
    drawSpectrum($('spectrum'), lab, { f0, sounding: S.hz });
    $('r-note').textContent = `${noteName(lab.note)} · ${f0.toFixed(1)} Hz`;
    $('r-hz').textContent = reg ? `${S.hz.toFixed(1)} Hz` : (lab.held ? 'no single pitch' : '—');
    $('r-cents').textContent = reg ? `${signed(1200 * Math.log2(S.hz / f0))} cents` : '—';
    $('r-m').textContent = ins ? `${ins.k[6]}` : '—';
    $('r-b').textContent = ins ? `${ins.k[7].toFixed(2)} / ${ins.k[1].toFixed(2)} / ${ins.k[8].toFixed(2)}` : '—';
    $('r-c').textContent = ins ? ins.k[4].toFixed(3) : '—';
    const rel = ins && S.h0 > 0 ? dB(S.hNow / S.h0) : null;
    $('r-h').textContent = rel === null ? '—' : `${signed(rel, 3)} dB`;
    hist.push({ h: rel });
    hist.draw([{ key: 'h', color: css('--trace'), lo: -60, hi: 20, width: 2 }], [[0, 'the strike', 'h'], [-30, '−30 dB', 'h']]);
    footer(lab, 'Toda ring');
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
  if (QS.has('demo')) ensureStarted(lab);
}

if (QS.get('gate') === 'toda') gateGuard('toda', gate); else wire();
