// flute.js — the Suzu lab's flute panel (Phase 8 steps 59b–59c, DECISIONS_7 #28–#31).
//
// A bore and a jet with no moving parts (SYNTH §2.11, §2.13): the bore's standing wave and
// its envelope, the jet swinging across the labium, the sound's phase plane and spectrum, the
// pitch it hears. ?gate=overblow renders a breath ramp offline through the same worklet and
// checks the overblow; ?demo=<breath> starts by itself, A4 held.
import { QS, $, css, noteName, noteHz, clamp, Lab, bindPower, ensureStarted, showNote, buildKeys, markKeys, bindKeymap, fit, drawSpectrum,
  Portrait, Strip, banner, footer, navigation, offlineRun, at, pitchAt, gateGuard, BoreView, audioOrbit, register, signed } from './lab-core.js';

const KIND = 6, P_REF = 0.005;
const lab = new Lab({ params: { voice_kind: KIND }, traceMask: 1 << KIND, traceDecim: 8, envelope: true, snapHz: 60 });
const S = { ins: null, envRms: null, breath: 70, ramp: null, hz: 0, midi: null };

// ---- the browser check: the breath ramp, offline ----
async function gate() {
  const sr = 48000, rampS = 12, holdS = 3, secs = rampS + holdS, note = 69, f0 = noteHz(note);
  const rampBlocks = rampS * sr / 128, blocks = Math.ceil(sr * secs / 128);
  const breathAt = (b) => Math.min(127, Math.round(127 * b / rampBlocks));
  const script = [[0, 0xB1, 2, 0], [0, 0x91, note, 100]];
  let last = 0; for (let b = 1; b < blocks; b++) { const v = breathAt(b); if (v !== last) { script.push([b, 0xB1, 2, v]); last = v; } }
  const r = await offlineRun({ params: { voice_kind: KIND }, script, seconds: secs, snapHz: 4 });
  const rows = [];
  for (let t = 0.5; t + 0.05 <= secs; t += 0.25) {
    const hz = pitchAt(r.x, sr, t), reg = register(hz, f0);
    rows.push({ t: +t.toFixed(2), breath: breathAt(t * sr / 128), hz: +hz.toFixed(2), register: reg ? reg.reg : 0, cents: reg ? +reg.cents.toFixed(1) : null });
  }
  const finite = r.x.every(Number.isFinite);
  const mid = rows.filter((q) => q.breath >= 40 && q.breath <= 90 && q.register === 1);
  const top = rows.filter((q) => q.t >= secs - 1);
  const octave = top.filter((q) => q.register === 2 && Math.abs(q.cents) <= 60);
  const jump = rows.find((q) => q.register >= 2);
  const near = (b) => rows.filter((q) => Math.abs(q.breath - b) <= 3 && q.hz > 0);
  const mean = (a) => a.reduce((s, q) => s + q.cents, 0) / (a.length || 1);
  const soft = mean(near(32)), ref = mean(near(56));
  const perSecond = rows.filter((q) => Math.abs(q.t - Math.round(q.t)) < 0.01).map((q) => `${q.breath}:${q.register ? `${q.register}×${signed(q.cents, 1)}` : 'silent'}`).join('  ');
  return { checks: [
    { name: 'the audio is finite', pass: finite, detail: `${secs} s rendered offline in ${r.renderMs.toFixed(0)} ms` },
    { name: 'mid-ramp on the note (the first register within 30 cents)', pass: mid.some((q) => Math.abs(q.cents) <= 30), detail: `${mid.length} windows in the first register at breath 40–90, the closest ${mid.length ? Math.min(...mid.map((q) => Math.abs(q.cents))).toFixed(1) : '—'} cents` },
    { name: 'the overblow: the last second on the octave (within 60 cents)', pass: octave.length >= Math.ceil(top.length / 2), detail: `${octave.length} of ${top.length} windows; the jump at ${jump ? `breath ${jump.breath}/127, t ${jump.t} s` : 'none'}` },
    { name: 'soft blowing flattens (breath 32 below breath 56 by 10 cents)', pass: soft < ref - 10, detail: `${soft.toFixed(1)} against ${ref.toFixed(1)} cents` },
  ], data: { rows, perSecond } };
}

// ---- the live page ----
function sendBreath(v) { S.breath = v; $('breath').value = v; $('breath-out').value = v; lab.cc(2, v); }
async function hold(note) { await ensureStarted(lab); lab.cc(2, S.breath); lab.hold(note); markKeys($('keys'), note, true); }
function release() { lab.release(); stopRamp(); markKeys($('keys'), 0, false); }
function stopRamp() { if (S.ramp) { clearInterval(S.ramp); S.ramp = null; $('ramp').textContent = 'Ramp the breath'; } }
async function startRamp() {
  if (S.ramp) { stopRamp(); return; }
  if (!lab.held) await hold(lab.note === 60 ? 69 : lab.note);
  const t0 = performance.now(); sendBreath(0); $('ramp').textContent = 'Stop the ramp';
  S.ramp = setInterval(() => { const v = Math.min(127, Math.round(127 * (performance.now() - t0) / 12000)); if (v !== S.breath) sendBreath(v); if (v >= 127) stopRamp(); }, 30);
}

const PARAMS = ['jet_tau', 'jet_gain', 'breath_range', 'bore_wall_s'];
function syncParams() {
  for (const p of PARAMS) { const el = $(`p-${p}`); if (!el || lab.params[p] === undefined) continue; el.value = lab.params[p]; el.nextElementSibling.value = (+lab.params[p]).toFixed(p === 'jet_gain' ? 0 : 2); }
}
let paramTimer = null;
function onParam(p, el) {
  el.nextElementSibling.value = (+el.value).toFixed(p === 'jet_gain' ? 0 : 2);
  clearTimeout(paramTimer);
  paramTimer = setTimeout(async () => { const r = await lab.setParams({ [p]: +el.value }); banner('refused', r.ok ? '' : `The engine refused the patch: ${r.log.trim()}`); syncParams(); }, 120);
}

async function webMidi(on) {
  if (!on) { if (S.midi) for (const i of S.midi.inputs.values()) i.onmidimessage = null; return; }
  if (!navigator.requestMIDIAccess) { showNote('This browser has no Web MIDI.'); $('webmidi').checked = false; return; }
  try {
    await ensureStarted(lab);
    S.midi = await navigator.requestMIDIAccess();
    const attach = () => { for (const i of S.midi.inputs.values()) i.onmidimessage = (e) => { const d = e.data; if (d.length >= 2 && d[0] < 0xF0) lab.send([[d[0], d[1], d[2] ?? 0]]); }; };
    attach(); S.midi.onstatechange = attach;
    showNote(`Listening to ${S.midi.inputs.size} MIDI input(s).`);
  } catch (err) { showNote(`Web MIDI refused: ${err.message}`); $('webmidi').checked = false; }
}

// the jet at the blown end: the delay line read along it, scaled to its displacement at the labium (where the engine
// applies the gain), compressed past three jet widths so the swing stays on the page; the share entering the bore
function drawJet(g, ins, jetW, yc, R, narrowAll) {
  const xf = 14, xe = jetW - 2, jw = Math.max(4, R * 0.11), narrow = jetW < 120;
  const y0 = ins ? ins.k[5] : 0.3, yEdge = yc - y0 * jw;
  g.strokeStyle = css('--tube'); g.lineWidth = 2;
  g.beginPath(); g.moveTo(2, yc - jw * 1.2); g.lineTo(xf, yc - jw * 0.6); g.moveTo(2, yc + jw * 1.2); g.lineTo(xf, yc + jw * 0.6); g.stroke();
  g.fillStyle = css('--edge');
  g.beginPath(); g.moveTo(xe - 10, yEdge); g.lineTo(xe + 2, yEdge - 9); g.lineTo(xe + 2, yEdge + 9); g.closePath(); g.fill();
  g.fillStyle = css('--muted'); g.font = '11px system-ui, sans-serif'; g.textAlign = 'center';
  g.fillText('out', xe - 26, yEdge - 22); g.fillText(narrow ? 'in' : 'into the bore', xe - (narrow ? 22 : 40), yEdge + 30);
  g.fillText('flue', xf - 4, yc - jw * 1.6 - 4);
  if (!ins) return;
  const comp = (v) => 3 * Math.tanh(v / 3), gain = ins.k[4], pts = [];
  for (let i = 0; i < 64; i++) { const x = i / 63; pts.push([xf + (xe - 10 - xf) * x, yc - comp(-gain * ins.c[i] * x) * jw]); }
  g.strokeStyle = css('--jet'); g.globalAlpha = 0.25; g.lineWidth = jw * 1.6; g.lineCap = 'round';
  g.beginPath(); for (const [x, y] of pts) g.lineTo(x, y); g.stroke();
  g.globalAlpha = 1; g.lineWidth = 2; g.beginPath(); for (const [x, y] of pts) g.lineTo(x, y); g.stroke();
  const fin = 0.5 * (1 - Math.tanh(ins.k[9] - y0)), bx = 6, by = yc + R + 6, bw = jetW - 24;
  g.fillStyle = css('--rule'); g.fillRect(bx, by, bw, 5); g.fillStyle = css('--jet'); g.fillRect(bx, by, bw * fin, 5);
  g.fillStyle = css('--muted'); g.textAlign = 'left'; g.fillText(narrow ? `${Math.round(100 * fin)} % in` : `${Math.round(100 * fin)} % of the jet in`, bx, by + 18);
}

function wire() {
  navigation('flute');
  const bore = new BoreView($('bore'), { pRef: P_REF, leftLabel: 'embouchure end (the labium)', rightLabel: 'open foot' });
  const phase = new Portrait($('phase')), hist = new Strip($('history'), 12);
  lab.on('ready', () => { $('engine-line').textContent = `Voxo ${lab.version}, the flute (voice kind ${KIND}), ${lab.sampleRate} Hz`; syncParams(); if (QS.has('demo')) { sendBreath(Number(QS.get('demo') || 70)); hold(69); } });
  lab.on('snap', (m) => {
    const fl = m.inspect.filter((v) => v.kind === KIND);
    S.ins = fl.find((v) => v.held) || fl[fl.length - 1] || null;
    S.envRms = m.env;
    if (m.log) showNote(m.log.trim().split('\n').pop());
  });
  buildKeys($('keys'), { lo: 60, hi: 96, onDown: (m) => hold(m) });
  bindKeymap(72, (m) => hold(m));
  bindPower(lab, $('power'));
  $('volume').addEventListener('input', (e) => lab.gain(+e.target.value));
  $('breath').addEventListener('input', (e) => { stopRamp(); sendBreath(+e.target.value); });
  $('ramp').addEventListener('click', startRamp);
  $('stop').addEventListener('click', release);
  for (const p of PARAMS) { const el = $(`p-${p}`); el.addEventListener('input', () => onParam(p, el)); }
  $('webmidi').addEventListener('change', (e) => webMidi(e.target.checked));
  const tick = () => {
    lab.readAudio(); S.hz = lab.pitch();
    const f0 = noteHz(lab.note), ins = S.ins;
    bore.draw(ins, S.envRms, drawJet, lab.ctx ? 'Hold a note — the bore appears when it sounds.' : 'Start the sound, then hold a note.');
    phase.draw([audioOrbit(lab, S.hz > 0 ? S.hz : f0)], { fade: 0.10, xLabel: 's', yLabel: 'ṡ / ω' });
    drawSpectrum($('spectrum'), lab, { f0, sounding: S.hz });
    $('r-note').textContent = `${noteName(lab.note)} · ${f0.toFixed(1)} Hz`;
    const reg = register(S.hz, f0);
    $('r-hz').textContent = reg ? `${S.hz.toFixed(1)} Hz` : (lab.held ? 'silent' : '—');
    $('r-reg').textContent = reg ? reg.name : '—';
    $('r-cents').textContent = reg ? `${signed(reg.cents)} cents` : '—';
    $('r-pm').textContent = ins ? `${(ins.k[1] / P_REF).toFixed(2)} P_ref` : '—';
    $('r-tau').textContent = ins ? `${(ins.k[3] * ins.freq / ins.rate2).toFixed(2)} periods` : '—';
    hist.push({ cents: S.hz > 0 ? 1200 * Math.log2(S.hz / f0) : null, breath: S.breath });
    hist.draw([{ key: 'breath', color: css('--breath'), lo: 0, hi: 127, width: 1, alpha: 0.7 }, { key: 'cents', color: css('--trace'), lo: -500, hi: 2100, width: 2 }],
      [[0, 'the note', 'cents'], [1200, 'octave', 'cents'], [1902, 'twelfth', 'cents']]);
    footer(lab, 'flute');
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
  if (QS.has('demo')) ensureStarted(lab);
}

if (QS.get('gate') === 'overblow') gateGuard('flute', gate); else wire();
