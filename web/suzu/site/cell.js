// cell.js — the Suzu lab's cell and shears (Phase 8 step 59c, DECISIONS_7 #31; SYNTH §2.1–§2.4).
//
// Voice kind 0: one magic-circle cell. The orbit is the cell's own (x, y) from the ring at full
// density; its size is x² + y² − εxy, the quantity the leapfrog keeps exactly. ?gate=cell checks
// the orbit's size, the shear's harmonics and the naive update's blow-up offline.
import { QS, $, css, noteName, noteHz, clamp, Lab, bindPower, ensureStarted, showNote, buildKeys, markKeys, bindKeymap, drawSpectrum,
  Portrait, Strip, banner, footer, navigation, offlineRun, at, gateGuard, signed } from './lab-core.js';

const KIND = 0;
const base = { voice_kind: KIND, decay_s: 0, shear: 0, shear_kind: 0, update_mode: 0, retune_mode: 0 };
const lab = new Lab({ params: { ...base }, traceMask: 1 << KIND, traceDecim: 2, recentPoints: 1023, snapHz: 60 });
const S = { pts: [], eps: 0, amp: 0, e0: 0, eNow: 0, serial: -1, glide: null };

const energy = (x, y, eps) => x * x + y * y - eps * x * y;
const dB = (r) => 10 * Math.log10(Math.max(r, 1e-30));

// ---- the browser check ----
function goertzel(x, f, sr) { const w = 2 * Math.PI * f / sr, c = 2 * Math.cos(w); let s1 = 0, s2 = 0; for (let i = 0; i < x.length; i++) { const s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; } return Math.sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / x.length; }
async function gate() {
  const note = 57, f0 = noteHz(note), script = [[0, 0x91, note, 100]];
  const sizes = (r) => { const eps = r.snaps.find((s) => s.inspect.length)?.inspect[0].k[0] ?? 0; const out = []; for (const s of r.snaps) { if (!s.recent || s.recent.length < 4) continue; const n = s.recent.length; out.push(energy(s.recent[n - 4], s.recent[n - 3], eps)); } return out; };
  const a = await offlineRun({ params: { ...base }, script, seconds: 3, traceMask: 1 << KIND, traceDecim: 2, recentPoints: 1023, snapHz: 20 });
  const ea = sizes(a), drift = ea.length > 12 ? Math.abs(dB(ea[ea.length - 1] / ea[10])) : Infinity;
  const harm = async (shear) => { const r = await offlineRun({ params: { ...base, shear }, script, seconds: 1.0 }); const seg = r.x.subarray(Math.floor(0.5 * r.sr)); return 20 * Math.log10(goertzel(seg, 3 * f0, r.sr) / goertzel(seg, f0, r.sr)); };
  const h0 = await harm(0), h5 = await harm(0.5);
  const n = await offlineRun({ params: { ...base, update_mode: 1 }, script, seconds: 3, traceMask: 1 << KIND, traceDecim: 2, recentPoints: 1023, snapHz: 20 });
  const en = sizes(n), grew = en.length > 2 ? dB(Math.max(...en) / en[0]) : 0;
  return { checks: [
    { name: 'the leapfrog keeps the orbit\'s size (the undamped cell, A3, 2.5 s)', pass: drift < 0.01, detail: `x² + y² − εxy drifts ${drift.toExponential(2)} dB over ${ea.length - 11} snapshots` },
    { name: 'the shear combs harmonics in (the 3rd against the fundamental)', pass: h0 < -60 && h5 > -40, detail: `${h0.toFixed(1)} dB at shear 0, ${h5.toFixed(1)} dB at shear 0.5` },
    { name: 'the naive update grows without bound — and the lab catches it (the red control)', pass: n.blowups.length > 0 && n.restarts.length > 0, detail: n.blowups.length ? `the lab caught it at quantum ${n.blowups[0].block} (${(n.blowups[0].block * 128 / n.sr).toFixed(2)} s) and restarted the engine ${n.restarts.length}×; the orbit's size had grown ${grew.toFixed(0)} dB by then` : `not caught (grew ${grew.toFixed(1)} dB)` },
  ], data: { drift, h0, h5 } };
}

// ---- the live page ----
async function hold(note) { await ensureStarted(lab); lab.velocity = +$('velocity').value; lab.hold(note); S.serial = -1; markKeys($('keys'), note, true); }
function release() { lab.release(); markKeys($('keys'), 0, false); }
function sliderOut(el, digits = 2) { el.nextElementSibling.value = (+el.value).toFixed(digits); }
async function apply(values) {
  const r = await lab.setParams(values);
  if (!r.ok) banner('refused', `The engine refused the patch: ${r.log.trim()}`); else if (!$('naive').checked) banner();
  return r;
}
function glide() {
  if (S.glide) return;
  if (!lab.held) hold(57);
  const t0 = performance.now();
  S.glide = setInterval(() => {
    const t = (performance.now() - t0) / 1000, s = t < 1.5 ? 24 * t / 1.5 : t < 3 ? 24 * (3 - t) / 1.5 : 0;
    lab.bend(s); if (t >= 3) { clearInterval(S.glide); S.glide = null; lab.bend(0); }
  }, 16);
}

function wire() {
  navigation('cell');
  const orbit = new Portrait($('orbit')), size = new Strip($('size'), 12);
  lab.on('ready', () => { $('engine-line').textContent = `Voxo ${lab.version}, one cell (voice kind ${KIND}), ${lab.sampleRate} Hz`; if (QS.has('demo')) { $('shear').value = 0.35; sliderOut($('shear')); apply({ shear: 0.35 }); hold(57); } });
  lab.on('snap', (m) => {
    const ins = m.inspect[0];
    if (ins) {
      S.eps = ins.k[0]; S.amp = ins.k[1];
      if (ins.serial !== S.serial) { S.serial = ins.serial; S.e0 = 0; }
    }
    if (m.recent && m.recent.length >= 8 && ins) {
      const r = m.recent, pts = []; for (let i = 0; i < r.length; i += 4) pts.push([r[i], r[i + 1]]);
      S.pts = pts.slice(-Math.min(pts.length, Math.round(2 * ins.rate2 / 2 / Math.max(ins.freq, 1)) + 2));   // two periods (density 2)
      // the size averaged over a whole number of periods: a sheared orbit is not a circle, so one point's
      // x² + y² − εxy wobbles with the phase; the mean over full turns is what drifts, or does not
      const per = Math.max(2, Math.round(ins.rate2 / 2 / Math.max(ins.freq, 1))), whole = Math.floor(pts.length / per) * per;
      let acc = 0; const from = pts.length - (whole || pts.length);
      for (let i = from; i < pts.length; i++) acc += energy(pts[i][0], pts[i][1], S.eps);
      S.eNow = acc / (pts.length - from);
      if (!S.e0 && S.eNow > 0) S.e0 = S.eNow;
    } else if (!ins) { S.pts = []; }
  });
  lab.on('blowup', (m) => { banner('red', `It blew up ${(m.block * 128 / lab.sampleRate).toFixed(2)} s into the engine's life: the naive update multiplies the orbit's area by 1 + ε² every sub-step, so the cell grew until its samples were no longer numbers. The lab muted it and restarted the engine — switch the naive update off to play again.`); });
  buildKeys($('keys'), { lo: 48, hi: 84, onDown: (m) => hold(m) });
  bindKeymap(60, (m) => hold(m));
  bindPower(lab, $('power'));
  $('volume').addEventListener('input', (e) => lab.gain(+e.target.value));
  for (const id of ['shear', 'decay', 'velocity']) sliderOut($(id), id === 'velocity' ? 0 : 2);
  $('shear').addEventListener('input', (e) => { sliderOut(e.target); apply({ shear: +e.target.value }); });
  $('decay').addEventListener('input', (e) => { sliderOut(e.target); apply({ decay_s: +e.target.value }); });
  $('velocity').addEventListener('input', (e) => { sliderOut(e.target, 0); lab.velocity = +e.target.value; });
  for (const b of $('shear-kind').querySelectorAll('button')) b.addEventListener('click', () => { for (const o of $('shear-kind').querySelectorAll('button')) o.classList.toggle('on', o === b); apply({ shear_kind: +b.dataset.v }); });
  $('naive').addEventListener('change', async (e) => { await apply({ update_mode: e.target.checked ? 1 : 0 }); if (e.target.checked) banner('red', 'The naive update is on. Hold a note and watch the orbit spiral out; the size over time climbs until the lab catches the blow-up.'); else banner(); });
  $('retune').addEventListener('change', (e) => apply({ retune_mode: +e.target.value }));
  $('glide').addEventListener('click', glide);
  $('stop').addEventListener('click', release);
  const tick = () => {
    lab.readAudio();
    const f0 = noteHz(lab.note);
    orbit.draw([S.pts], { fade: 0.18, xLabel: 'x', yLabel: 'y', guide: S.amp > 0 && lab.held ? S.amp : null });
    drawSpectrum($('spectrum'), lab, { f0, sounding: lab.pitch() });
    const rel = S.e0 > 0 && lab.held ? dB(S.eNow / S.e0) : null;
    $('r-note').textContent = `${noteName(lab.note)} · ${f0.toFixed(1)} Hz`;
    $('r-eps').textContent = S.eps ? S.eps.toFixed(5) : '—';
    $('r-drift').textContent = rel === null ? '—' : `${signed(rel, 4)} dB`;
    size.push({ e: rel });
    size.draw([{ key: 'e', color: css('--trace'), lo: -6, hi: 30, width: 2 }], [[0, 'the size at the strike', 'e'], [20, '+20 dB', 'e']]);
    footer(lab, 'cell');
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
  if (QS.has('demo')) ensureStarted(lab);
}

if (QS.get('gate') === 'cell') gateGuard('cell', gate); else wire();
