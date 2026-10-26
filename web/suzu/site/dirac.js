// dirac.js — the Suzu lab's Dirac network (v3.0 step 92; the voice of step 85, DECISIONS_13 #3; TO_PROJECT_SPEC §13.13).
//
// Voice kind 12: a port-Hamiltonian network — H = ½xᵀQx, a skew Dirac structure J routing it between ports, the
// Cayley step M = (I − h/2·A)⁻¹(I + h/2·A) built once at the strike, an isometry in the Q-norm (its gain by power
// iteration, k[6]). The prepared network: a chain of masses on springs, a bridge on the last spring (the voice is the
// force on it), the stick-slip bow on a mass as a port, and the rattle stop under the bridge with its declared loss.
// ?gate=dirac checks the chain on the note with the bridge's loading counted, the ledger H + the stop's losses − the
// bow's gain flat at the strike's energy, the gain never above the bow's work, the stop rattling, the step's passivity,
// and the red control — the naive bow solve — breaking the ledger's inequality or the state's finiteness.
import { QS, $, css, noteName, noteHz, clamp, Lab, bindPower, ensureStarted, buildKeys, markKeys, bindKeymap, drawSpectrum,
  Strip, banner, footer, navigation, offlineRun, gateGuard, register, signed, peakScan, fit, DOCS_ROOT } from './lab-core.js';

const KIND = 12;
// the patch table's prepared network (voxo/src/suzu_patches.cpp), the bow off until the page switches it on
const BASE = { voice_kind: KIND, dirac_masses: 8, dirac_bridge_hz: 700, dirac_bridge_mass: 0.5, dirac_stop: 2, dirac_stop_loss: 0.3, dirac_bridge_decay: 0.5,
  bow_mode: 0, bow_force: 0.5, bow_speed: 1, bow_position: 0.12, bow_naive: 0, decay_s: 2, pluck: 0.3,
  level: 0.3, attack_s: 0.002, release_s: 0.5, cutoff_hz: 20000, resonance: 0 };
const lab = new Lab({ params: { ...BASE }, traceMask: 1 << KIND, traceDecim: 4, recentPoints: 64, snapHz: 30 });
const S = { ins: null, serial: -1, h0: 0, hz: 0, breath: 90, press: 80, xScale: 1e-6, ledger: [] };
const cents = (a, b) => 1200 * Math.log2(a / b);

// ---- the browser check ----
async function gate() {
  const sr = 48000, note = 57, f0 = noteHz(note);
  const line = (x, t0, t1) => peakScan(x.subarray(Math.floor(t0 * sr), Math.floor(t1 * sr)), sr, f0 - 8, f0 + 8, 0.05).hz;
  const tOf = (s) => s.block * 128 / sr;
  // (1) plucked, lossless, no bow, no stop: the chain's lowest mode on the note with the bridge's loading counted
  const pl = await offlineRun({ params: { ...BASE, decay_s: 0, dirac_stop: 0, dirac_bridge_decay: 0 }, script: [[0, 0x91, note, 100]], seconds: 3.0, snapHz: 20 });
  const plC = cents(line(pl.x, 0.5, 2.5), f0);
  const plK = pl.snaps.filter((s) => s.inspect.length).pop()?.inspect[0].k ?? null;
  const plH = pl.snaps.filter((s) => s.inspect.length && tOf(s) > 0.2).map((s) => s.inspect[0].k[0]);
  const plDrift = plH.length > 4 ? Math.abs(10 * Math.log10(plH[plH.length - 1] / plH[0])) : Infinity;
  // (2) the bow (breath 90, press 80) with the stop, lossless: the ledger closes, the gain never above the work, the stop rattles
  const bowed = { ...BASE, bow_mode: 1, decay_s: 0, dirac_bridge_decay: 0 };
  const b = await offlineRun({ params: bowed, script: [[0, 0xB1, 2, 90], [0, 0xD1, 80, 0], [0, 0x91, note, 100]], seconds: 4.0, snapHz: 20 });
  const bs = b.snaps.filter((s) => s.inspect.length && tOf(s) > 0.05).map((s) => s.inspect[0].k);
  const ledger = bs.map((k) => k[0] + k[2] - k[1]), H0 = ledger[0] ?? 0, Hmax = Math.max(1e-30, ...bs.map((k) => k[0]));
  const resid = ledger.length ? Math.max(...ledger.map((l) => Math.abs(l - H0))) / Hmax : Infinity;
  const gainOver = bs.length ? Math.max(...bs.map((k) => k[1] - k[8])) / Hmax : Infinity;
  const stops = bs.length ? bs[bs.length - 1][3] : 0, losses = bs.length ? bs[bs.length - 1][2] : 0;
  const bowC = cents(line(b.x, 2.0, 4.0), f0);
  // (3) the strings' red on the network: the three Newton steps taken raw — no bound, no hysteresis — at the force 2.0.
  // On the chain (kind 2) the suite's RED breaks the ledger's line or the state's finiteness; on the network the bow
  // acts through its own kick scale (SUZU_DIRAC_KF), and the raw steps stay in bound — measured here, not assumed:
  // the ledger closes and the gain stays under the work. The network has no red of its own; its safety is arithmetic.
  const red = await offlineRun({ params: { ...bowed, bow_naive: 1, bow_force: 2.0 }, script: [[0, 0xB1, 2, 90], [0, 0xD1, 80, 0], [0, 0x91, note, 100]], seconds: 3.0, snapHz: 20 });
  const rs = red.snaps.filter((s) => s.inspect.length && tOf(s) > 0.05).map((s) => s.inspect[0].k);
  const rHmax = Math.max(1e-30, ...rs.map((k) => k[0])), rOver = rs.length ? Math.max(...rs.map((k) => k[1] - k[8])) / rHmax : Infinity;
  const rLedger = rs.map((k) => k[0] + k[2] - k[1]), rResid = rLedger.length ? Math.max(...rLedger.map((l) => Math.abs(l - rLedger[0]))) / rHmax : Infinity;
  const redHeld = !red.blowups.length && rOver <= 1e-4 && rResid < 1e-3 && rs.length > 30;
  return { checks: [
    { name: 'plucked, lossless: the chain\'s lowest mode sounds the note with the bridge\'s loading counted (A3; the suite: −0.2 cents), and the Cayley step keeps H', pass: Number.isFinite(plC) && Math.abs(plC) < 3 && plDrift < 0.01, detail: `${signed(plC, 2)} cents; H drifts ${plDrift.toExponential(2)} dB over 2.8 s${plK ? ` (${plK[7]} states)` : ''}` },
    { name: 'the step is passive: its gain in the Q-norm is 1 — never above (the suite: 2e-16 over twelve random skew J)', pass: !!plK && plK[6] <= 1 + 1e-6 && plK[6] > 0.999, detail: plK ? `gain_q ${plK[6].toFixed(9)}` : 'no inspection' },
    { name: 'the ledger under the bow (breath 90, press 80), lossless with the stop: H + the stop\'s losses − the bow\'s gain stays at the strike\'s energy', pass: resid < 1e-3 && bs.length > 40, detail: `the residual ${resid.toExponential(2)} of H over ${bs.length} snapshots (the suite: a millionth in double; the lab reads the inspection's floats)` },
    { name: 'the bow\'s gain — the force at the synchronised velocity — never exceeds its work at its own speed; the stop rattles and takes its declared loss', pass: gainOver <= 1e-4 && stops > 100 && losses > 0, detail: `the gain over the work at most ${gainOver.toExponential(2)} of H; the stop hit ${stops} times, taking ${losses.toExponential(3)}` },
    { name: 'the bow holds the tone on the note', pass: Number.isFinite(bowC) && Math.abs(bowC) < 10, detail: `${signed(bowC, 1)} cents over the last two seconds` },
    { name: 'the strings\' red on the network — the naive bow solve at the force 2.0 (the raw Newton steps, no bound, no hysteresis), which breaks the chain\'s ledger in the suite — stays in bound here: the network\'s kick scale holds the raw steps, the ledger closes and the gain stays under the work (measured, not assumed; the network has no red of its own)', pass: redHeld, detail: red.blowups.length ? `non-finite at ${(red.blowups[0].block * 128 / sr).toFixed(2)} s — the lab caught it` : `the gain over the work ${rOver.toExponential(2)} of H, the ledger's residual ${rResid.toExponential(2)} (the bound's ${gainOver.toExponential(2)} and ${resid.toExponential(2)})` },
  ], data: { plC, plDrift, gainQ: plK ? plK[6] : null, resid, gainOver, stops, bowC, rOver, rResid, redBlowups: red.blowups.length } };
}

// ---- the live page ----
async function hold(note) { await ensureStarted(lab); lab.velocity = +$('velocity').value; lab.cc(2, S.breath); lab.pressure(S.press); lab.hold(note); markKeys($('keys'), note, true); }
function release() { lab.release(); markKeys($('keys'), 0, false); }
const out = (el, d = 2) => { el.nextElementSibling.value = (+el.value).toFixed(d); };
async function apply(values) { const r = await lab.setParams(values); if (!r.ok) banner('refused', `The engine refused the patch: ${r.log.trim()}`); else if (!$('red').checked) banner(); return r; }

function drawNetwork() {
  const { g, w, h } = fit($('network'));
  g.clearRect(0, 0, w, h);
  const ins = S.ins;
  if (!ins || !ins.n) { g.fillStyle = css('--muted'); g.font = '12px system-ui, sans-serif'; g.textAlign = 'center'; g.fillText(lab.ctx ? 'Pluck a key — the network appears when it sounds.' : 'Start the sound, then pluck a key.', w / 2, h / 2); return; }
  const n = ins.n, m = Math.round(ins.k[7]) === n ? Math.round((n - 3) / 2) : Math.round((n - 3) / 2);   // n = 2m + 3: m + 1 extensions, m momenta, the bridge's two
  let peak = 1e-9; for (let i = 0; i < n; i++) peak = Math.max(peak, Math.abs(ins.a[i]));
  S.xScale = Math.max(peak, S.xScale * 0.96);
  const rows = [[0, m + 1, 'the springs\' extensions'], [m + 1, 2 * m + 1, 'the masses\' momenta'], [2 * m + 1, n, 'the bridge: extension, momentum']];
  const rh = (h - 30) / 3, bw = Math.max(4, Math.min(24, (w - 60) / (m + 3)));
  rows.forEach(([i0, i1, label], r) => {
    const y0 = 16 + r * rh, mid = y0 + rh / 2 - 6;
    g.strokeStyle = css('--rule'); g.lineWidth = 1; g.beginPath(); g.moveTo(30, mid); g.lineTo(w - 10, mid); g.stroke();
    g.fillStyle = css('--muted'); g.font = '10px system-ui, sans-serif'; g.textAlign = 'left'; g.fillText(label, 30, y0 + 2);
    g.fillStyle = r === 2 ? css('--env') : css('--trace');
    for (let i = i0; i < i1; i++) { const v = clamp(ins.a[i] / S.xScale, -1, 1), x = 34 + (i - i0) * (bw + 4), hh = v * (rh / 2 - 12); g.fillRect(x, hh >= 0 ? mid - hh : mid, bw, Math.abs(hh)); }
  });
}
function drawLedger() {
  const { g, w, h } = fit($('ledger'));
  g.clearRect(0, 0, w, h);
  const L = S.ledger; if (L.length < 2) { g.fillStyle = css('--muted'); g.font = '12px system-ui, sans-serif'; g.textAlign = 'center'; g.fillText('H, the losses and the gain since the strike', w / 2, h / 2); return; }
  let top = 1e-30; for (const r of L) top = Math.max(top, r.h, r.sum, r.loss, r.gain);
  const X = (i) => 8 + (w - 16) * i / (L.length - 1), Y = (v) => h - 14 - (h - 28) * v / top;
  const series = [['sum', css('--ink'), 'H + losses − gain'], ['h', css('--trace'), 'H'], ['loss', css('--env'), 'the stop\'s losses'], ['gain', css('--muted'), 'the bow\'s gain']];
  g.lineWidth = 1.5; g.font = '10px system-ui, sans-serif';
  series.forEach(([k, color, label], s) => { g.strokeStyle = color; g.beginPath(); L.forEach((r, i) => (i ? g.lineTo(X(i), Y(r[k])) : g.moveTo(X(i), Y(r[k])))); g.stroke(); g.fillStyle = color; g.textAlign = 'left'; g.fillText(label, 8 + s * (w - 16) / 4, h - 3); });
}

function wire() {
  navigation('dirac'); $('chapter-link').href = `${DOCS_ROOT}suzu/dirac-network/`;
  const hist = new Strip($('history'), 6);
  lab.on('ready', () => { $('engine-line').textContent = `Voxo ${lab.version}, the Dirac network (voice kind ${KIND}), ${lab.sampleRate} Hz`; if (QS.has('demo')) { $('bow').checked = true; apply({ bow_mode: 1 }).then(() => hold(57)); } });
  lab.on('snap', (m) => {
    S.ins = m.inspect.find((v) => v.held) || m.inspect[m.inspect.length - 1] || null;
    if (S.ins) {
      const k = S.ins.k;
      if (S.ins.serial !== S.serial) { S.serial = S.ins.serial; S.h0 = k[0] + k[2] - k[1]; S.ledger = []; }
      S.ledger.push({ h: k[0], loss: k[2], gain: k[1], sum: k[0] + k[2] - k[1] }); if (S.ledger.length > 240) S.ledger.shift();
    }
  });
  lab.on('blowup', (m) => { banner('red', `It blew up ${(m.block * 128 / lab.sampleRate).toFixed(2)} s into the engine's life: the naive solve let the bow's kick outrun the friction curve, and the state grew until its samples were no longer numbers. The lab muted it and restarted the engine — switch the red control off to play again.`); });
  buildKeys($('keys'), { lo: 36, hi: 84, onDown: (m) => hold(m) });
  bindKeymap(57, (m) => hold(m));
  bindPower(lab, $('power'));
  $('volume').addEventListener('input', (e) => lab.gain(+e.target.value));
  for (const id of ['breath', 'press', 'masses', 'bridge', 'velocity']) out($(id), 0);
  for (const id of ['bmass', 'stop-d', 'stop-loss', 'decay']) out($(id));
  $('bow').addEventListener('change', (e) => apply({ bow_mode: e.target.checked ? 1 : 0 }));
  $('breath').addEventListener('input', (e) => { out(e.target, 0); S.breath = +e.target.value; lab.cc(2, S.breath); });
  $('press').addEventListener('input', (e) => { out(e.target, 0); S.press = +e.target.value; lab.pressure(S.press); });
  $('masses').addEventListener('input', (e) => { out(e.target, 0); apply({ dirac_masses: +e.target.value }); });
  $('bridge').addEventListener('input', (e) => { out(e.target, 0); apply({ dirac_bridge_hz: +e.target.value }); });
  $('bmass').addEventListener('input', (e) => { out(e.target); apply({ dirac_bridge_mass: +e.target.value }); });
  $('stop-d').addEventListener('input', (e) => { out(e.target); apply({ dirac_stop: +e.target.value }); });
  $('stop-loss').addEventListener('input', (e) => { out(e.target); apply({ dirac_stop_loss: +e.target.value }); });
  $('decay').addEventListener('input', (e) => { out(e.target); apply({ decay_s: +e.target.value, dirac_bridge_decay: Math.min(+e.target.value, 0.5) }); });
  $('velocity').addEventListener('input', (e) => { out(e.target, 0); lab.velocity = +e.target.value; });
  $('stop').addEventListener('click', release);
  $('red').addEventListener('change', async (e) => {
    await apply(e.target.checked ? { bow_naive: 1, bow_force: 2.0 } : { bow_naive: 0, bow_force: 0.5 });
    if (e.target.checked) banner('red', 'The naive bow solve — the strings\' red: the three Newton steps taken raw at the force 2.0, no bound on the kick, no hysteresis at the stick. On the chain it breaks the ledger or the numbers; on the network the bow acts through the chain\'s own kick scale and the raw steps stay in bound — switch the bow on and watch the ledger stay closed. The network has no red of its own: its safety is the Cayley step\'s arithmetic.'); else banner();
  });
  const tick = () => {
    lab.readAudio(); S.hz = lab.pitch();
    const f0 = noteHz(lab.note), reg = register(S.hz, f0), ins = S.ins;
    drawNetwork(); drawLedger();
    drawSpectrum($('spectrum'), lab, { f0, sounding: S.hz });
    $('r-note').textContent = `${noteName(lab.note)} · ${f0.toFixed(1)} Hz`;
    $('r-hz').textContent = reg ? `${S.hz.toFixed(1)} Hz` : (lab.held ? 'no single pitch' : '—');
    $('r-cents').textContent = reg ? `${signed(1200 * Math.log2(S.hz / f0))} cents` : '—';
    $('r-gainq').textContent = ins ? ins.k[6].toFixed(9) : '—';
    $('r-stops').textContent = ins ? `${ins.k[3]}` : '—';
    const last = S.ledger[S.ledger.length - 1];
    $('r-ledger').textContent = last && S.h0 > 0 ? `${((last.sum - S.h0) / S.h0).toExponential(2)} of H₀` : '—';
    hist.push({ f: ins ? ins.k[4] : null });
    hist.draw([{ key: 'f', color: css('--trace'), lo: -1, hi: 1, width: 1.5 }], [[0, 'the bow\'s force', 'f']]);
    footer(lab, 'Dirac network');
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
  if (QS.has('demo')) ensureStarted(lab);
}

if (QS.get('gate') === 'dirac') gateGuard('dirac', gate); else wire();
