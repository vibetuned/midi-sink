// lab.js — the Suzu lab's page, the flute panel (Phase 8 step 59b, DECISIONS_7 #28).
//
// The page holds no physics: the AudioWorklet (suzu-worklet.js) runs the engine — Voxo's
// Suzu compiled to WebAssembly — and posts the orbit trace and the inspection about sixty
// times a second; the page draws them and reads the sounding pitch from the audio itself.
// ?gate=overblow renders a scripted breath ramp offline through the same worklet and posts
// the pitch it hears (tools/suzu_lab_gate.mjs); ?demo=<breath> starts by itself, A4 held.
import { KIND_NAMES, smoothAlong } from './suzu-engine.js';

const qs = new URLSearchParams(location.search);
const GATE = qs.get('gate');
const DEMO = qs.has('demo') ? Number(qs.get('demo') || 70) : null;
const P_REF = 0.005;                 // the flute's reference mouth pressure, in the bore's units (voxo.cpp)
const KIND = 6;                      // the flute
const CH = 1;                        // the MPE member channel the panel plays on (the master is 0)
const NOTE_LO = 60, NOTE_HI = 96;    // C4 … C7: the flute's range

// ---- the pitch, read from the sound: the autocorrelation's first lag within 0.08 of its maximum,
// refined by a parabola (the measure the desktop's calibration and gates use, DECISIONS_7 #21) ----
export function periodHz(x, rate, fmin = 70, fmax = 4000) {
  const n = x.length; let mean = 0; for (let i = 0; i < n; i++) mean += x[i]; mean /= n;
  let e0 = 0; for (let i = 0; i < n; i++) { const d = x[i] - mean; e0 += d * d; }
  if (e0 / n < 1e-9) return 0;
  const lo = Math.max(2, Math.floor(rate / fmax)), hi = Math.min(Math.floor(rate / fmin), (n >> 1) - 1), m = n - hi;
  const r = new Float32Array(hi + 2); let best = -2;
  let ea = 0; for (let i = 0; i < m; i++) { const d = x[i] - mean; ea += d * d; }
  for (let l = lo; l <= hi + 1; l++) {
    let c = 0, eb = 0;
    for (let i = 0; i < m; i++) { const a = x[i] - mean, b = x[i + l] - mean; c += a * b; eb += b * b; }
    r[l] = c / Math.sqrt((ea || 1e-30) * (eb || 1e-30)); if (l <= hi && r[l] > best) best = r[l];
  }
  if (best < 0.5) return 0;
  for (let l = lo + 1; l <= hi; l++) {
    if (r[l] >= best - 0.08 && r[l] >= r[l - 1] && r[l] >= r[l + 1]) {
      const den = r[l - 1] - 2 * r[l] + r[l + 1];
      const off = den !== 0 ? 0.5 * (r[l - 1] - r[l + 1]) / den : 0;
      return rate / (l + Math.max(-0.5, Math.min(0.5, off)));
    }
  }
  return 0;
}
const NAMES = ['C', 'C♯', 'D', 'E♭', 'E', 'F', 'F♯', 'G', 'A♭', 'A', 'B♭', 'B'];
const noteName = (m) => `${NAMES[m % 12]}${Math.floor(m / 12) - 1}`;
const noteHz = (m) => 440 * Math.pow(2, (m - 69) / 12);
const css = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();

// ---- the gate: the breath ramp rendered offline through the worklet, the pitch posted ----
async function runGate() {
  document.body.insertAdjacentHTML('afterbegin', '<p id="gate-line" style="padding:8px 16px">gate: rendering the breath ramp…</p>');
  const post = (url, body) => fetch(url, { method: 'POST', body: typeof body === 'string' ? body : JSON.stringify(body) }).catch(() => {});
  try {
    const sr = 48000, rampS = 12, holdS = 3, secs = rampS + holdS, note = 69;
    const wasm = await (await fetch('suzu.wasm')).arrayBuffer();
    const off = new OfflineAudioContext(2, sr * secs, sr);
    await off.audioWorklet.addModule('suzu-worklet.js');
    const blocks = Math.ceil(sr * secs / 128), rampBlocks = rampS * sr / 128;
    const script = [[0, 0xB0, 101, 0], [0, 0xB0, 100, 6], [0, 0xB0, 6, 15], [0, 0xB0 | CH, 2, 0], [0, 0x90 | CH, note, 100]];
    const breathAt = (b) => Math.min(127, Math.round(127 * b / rampBlocks));
    let last = 0; for (let b = 1; b < blocks; b++) { const v = breathAt(b); if (v !== last) { script.push([b, 0xB0 | CH, 2, v]); last = v; } }
    const costs = [];
    const node = new AudioWorkletNode(off, 'suzu', { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2],
      processorOptions: { wasm, params: { voice_kind: KIND }, traceMask: 0, segments: 8, snapHz: 4, script } });
    node.port.onmessage = (e) => { if (e.data.type === 'snap' && e.data.cost) costs.push(e.data.cost); };
    node.connect(off.destination);
    const t0 = performance.now();
    const buf = await off.startRendering();
    const renderMs = performance.now() - t0;
    const x = buf.getChannelData(0), f0 = noteHz(note), rows = [];
    for (let t = 0.5; t + 0.05 <= secs; t += 0.25) {
      const end = Math.floor(t * sr), win = x.subarray(end - 4096, end);
      const hz = periodHz(win, sr);
      let pk = 0; for (const v of win) pk = Math.max(pk, Math.abs(v));
      const breath = breathAt(end / 128);
      const reg = hz > 0 ? Math.max(1, Math.round(hz / f0)) : 0;
      rows.push({ t: +t.toFixed(2), breath, hz: +hz.toFixed(2), peak: +pk.toFixed(5), register: reg, cents: hz > 0 ? +(1200 * Math.log2(hz / (reg * f0))).toFixed(1) : null });
    }
    const finite = Array.from(x).every(Number.isFinite);
    const result = { gate: 'overblow', note, sampleRate: sr, seconds: secs, renderMs: +renderMs.toFixed(1), finite, rows, costs, userAgent: navigator.userAgent };
    document.getElementById('gate-line').textContent = `gate: done — ${rows.length} windows, rendered ${secs} s in ${renderMs.toFixed(0)} ms`;
    await post('/result', result);
  } catch (err) {
    document.getElementById('gate-line').textContent = `gate: failed — ${err.message}`;
    await post('/result', { gate: 'overblow', error: String(err && err.stack || err) });
  }
}

// ---- the live lab ----
const S = {
  ctx: null, node: null, analyser: null, gain: null, wasm: null, starting: false,
  note: 69, held: false, breath: 70, ramp: null, params: {}, ready: false,
  ins: null, envRms: null, trace: [], cost: null, version: '', sampleRate: 0,
  pScale: 1e-4, uScale: 1e-4,
  hz: 0, history: [], midiAccess: null,
};
const $ = (id) => document.getElementById(id);

function send(events) { if (S.node) S.node.port.postMessage({ type: 'midi', events }); }
function sendBreath(v) { S.breath = v; $('breath').value = v; $('breath-out').value = v; send([[0xB0 | CH, 2, v]]); }
function hold(note) {
  const ev = [];
  if (S.held) ev.push([0x80 | CH, S.note, 0]);
  S.note = note; S.held = true;
  ev.push([0xB0 | CH, 2, S.breath], [0x90 | CH, note, 100]);
  send(ev); markKeys();
}
function release() { if (S.held) send([[0x80 | CH, S.note, 0]]); S.held = false; stopRamp(); markKeys(); }
function stopRamp() { if (S.ramp) { clearInterval(S.ramp); S.ramp = null; $('ramp').textContent = 'Ramp the breath'; } }
function startRamp() {
  if (S.ramp) { stopRamp(); return; }
  if (!S.held) hold(S.note);
  const t0 = performance.now(); sendBreath(0);
  $('ramp').textContent = 'Stop the ramp';
  S.ramp = setInterval(() => {
    const v = Math.min(127, Math.round(127 * (performance.now() - t0) / 12000));
    if (v !== S.breath) sendBreath(v);
    if (v >= 127) stopRamp();
  }, 30);
}

async function power() {
  if (S.ctx) {
    if (S.ctx.state === 'running') { await S.ctx.suspend(); $('power').textContent = 'Resume the sound'; }
    else { await S.ctx.resume(); $('power').textContent = 'Pause the sound'; }
    return;
  }
  if (S.starting) return; S.starting = true;
  $('power').disabled = true; $('power').textContent = 'Starting…';
  try {
    S.wasm = await (await fetch('suzu.wasm')).arrayBuffer();
    S.ctx = new AudioContext({ latencyHint: 'interactive' });
    await S.ctx.audioWorklet.addModule('suzu-worklet.js');
    S.node = new AudioWorkletNode(S.ctx, 'suzu', { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2],
      processorOptions: { wasm: S.wasm, params: { voice_kind: KIND }, traceMask: 1 << KIND, segments: 16, snapHz: 60 } });
    S.node.port.onmessage = (e) => onMessage(e.data);
    S.gain = S.ctx.createGain(); S.gain.gain.value = Number($('volume').value);
    S.analyser = S.ctx.createAnalyser(); S.analyser.fftSize = 8192; S.analyser.smoothingTimeConstant = 0.55;
    S.node.connect(S.gain).connect(S.ctx.destination);
    S.node.connect(S.analyser);
    send([[0xB0, 101, 0], [0xB0, 100, 6], [0xB0, 6, 15], [0xB0 | CH, 2, S.breath]]);   // the MPE zone, the breath
    await S.ctx.resume();
    $('power').textContent = 'Pause the sound';
  } catch (err) {
    $('hint').textContent = `The sound could not start: ${err.message}. The lab needs a browser with AudioWorklet and WebAssembly.`;
    $('power').textContent = 'Start the sound';
  }
  $('power').disabled = false; S.starting = false;
}

function onMessage(m) {
  if (m.type === 'ready') {
    S.ready = true; S.version = m.version; S.sampleRate = m.sampleRate; S.params = m.params;
    $('engine-line').textContent = `Voxo ${m.version}, the flute (voice kind ${KIND}), ${m.sampleRate} Hz`;
    $('foot-engine').textContent = `Voxo ${m.version} in WebAssembly — Suzu's ${KIND_NAMES[KIND]}, no audio library: the worklet renders`;
    syncParams();
    if (DEMO !== null) { sendBreath(DEMO); hold(69); }
  } else if (m.type === 'snap') {
    const fl = m.inspect.filter((v) => v.kind === KIND);
    S.ins = fl.find((v) => v.held) || fl[fl.length - 1] || null;
    S.envRms = m.env && S.ins && m.env.length === S.ins.n ? m.env : null;
    if (m.trace.length) S.trace = m.trace;
    if (m.cost) S.cost = m.cost;
    if (m.log) $('hint').textContent = m.log.trim().split('\n').pop();
  } else if (m.type === 'applied') {
    S.params = m.params;
    if (!m.ok) $('hint').textContent = `The engine refused the patch: ${(m.log || '').trim()}`;
    syncParams();
  }
}

// ---- the embouchure's parameters ----
const PARAMS = ['jet_tau', 'jet_gain', 'breath_range', 'bore_wall_s'];
function syncParams() {
  for (const p of PARAMS) {
    const el = $(`p-${p}`); if (!el || S.params[p] === undefined) continue;
    el.value = S.params[p]; el.nextElementSibling.value = (+S.params[p]).toFixed(p === 'jet_gain' ? 0 : 2);
  }
}
let paramTimer = null;
function onParam(p, el) {
  el.nextElementSibling.value = (+el.value).toFixed(p === 'jet_gain' ? 0 : 2);
  clearTimeout(paramTimer);
  paramTimer = setTimeout(() => { if (S.node) S.node.port.postMessage({ type: 'params', values: { [p]: +el.value } }); }, 120);
}

// ---- the keyboard ----
function buildKeys() {
  const keys = $('keys'); keys.innerHTML = '';
  const black = (m) => [1, 3, 6, 8, 10].includes(m % 12);
  const whites = []; for (let m = NOTE_LO; m <= NOTE_HI; m++) if (!black(m)) whites.push(m);
  const W = whites.length;
  for (let m = NOTE_LO; m <= NOTE_HI; m++) {
    const k = document.createElement('div');
    k.className = 'key' + (black(m) ? ' black' : ''); k.dataset.note = m;
    k.textContent = m % 12 === 0 ? noteName(m) : '';
    k.title = `${noteName(m)} · ${noteHz(m).toFixed(1)} Hz`;
    if (black(m)) { const i = whites.indexOf(m - 1); k.style.left = `${((i + 1) / W - 0.3 / W) * 100}%`; k.style.width = `${(0.6 / W) * 100}%`; }
    k.addEventListener('pointerdown', (e) => { e.preventDefault(); if (!S.ctx) power().then(() => hold(m)); else hold(m); });
    keys.appendChild(k);
  }
  markKeys();
}
function markKeys() { for (const k of document.querySelectorAll('.key')) k.classList.toggle('on', S.held && +k.dataset.note === S.note); }
const KEYMAP = 'awsedftgyhujk';   // C5 … C6 on the computer's keyboard
window.addEventListener('keydown', (e) => {
  if (e.repeat || e.metaKey || e.ctrlKey || e.target.tagName === 'INPUT') return;
  const i = KEYMAP.indexOf(e.key.toLowerCase()); if (i < 0) return;
  const m = 72 + i; if (!S.ctx) power().then(() => hold(m)); else hold(m);
});

// ---- Web MIDI: a controller plays the panel (its breath on CC 2 or 11, its notes on any channel) ----
async function webMidi(on) {
  if (!on) { if (S.midiAccess) for (const i of S.midiAccess.inputs.values()) i.onmidimessage = null; return; }
  if (!navigator.requestMIDIAccess) { $('hint').textContent = 'This browser has no Web MIDI.'; $('webmidi').checked = false; return; }
  try {
    if (!S.ctx) await power();
    S.midiAccess = await navigator.requestMIDIAccess();
    const attach = () => { for (const i of S.midiAccess.inputs.values()) i.onmidimessage = (e) => { const d = e.data; if (d.length >= 2 && d[0] < 0xF0) send([[d[0], d[1], d[2] ?? 0]]); }; };
    attach(); S.midiAccess.onstatechange = attach;
    $('hint').textContent = `Listening to ${S.midiAccess.inputs.size} MIDI input(s).`;
  } catch (err) { $('hint').textContent = `Web MIDI refused: ${err.message}`; $('webmidi').checked = false; }
}

// ---- the drawing ----
function fit(cv) {
  const r = cv.getBoundingClientRect(), dpr = Math.min(2, window.devicePixelRatio || 1);
  const w = Math.max(1, Math.round(r.width * dpr)), h = Math.max(1, Math.round(r.height * dpr));
  if (cv.width !== w || cv.height !== h) { cv.width = w; cv.height = h; }
  const g = cv.getContext('2d'); g.setTransform(dpr, 0, 0, dpr, 0, 0);
  return { g, w: r.width, h: r.height };
}

function drawBore() {
  const { g, w, h } = fit($('bore'));
  g.clearRect(0, 0, w, h);
  const ins = S.ins;
  const jetW = Math.min(170, Math.max(90, w * 0.17)), x0 = jetW, x1 = w - 18, yc = h * 0.52, R = h * 0.27;
  // the envelope: the worklet's RMS per node over a quarter second (every quantum), drawn as √2·RMS — a sine's
  // peak — so the fundamental's half-sine reads as the envelope of the pressure swinging inside it
  const n = ins ? ins.n : 0;
  const env = new Float32Array(n);
  const pNow = ins ? smoothAlong(ins.a, n) : null, uNow = ins ? smoothAlong(ins.b, Math.max(0, n - 1)) : null;   // the drawing's average along the bore
  if (ins) {
    let pk = 0, upk = 0;
    for (let i = 0; i < n; i++) { env[i] = S.envRms ? Math.SQRT2 * S.envRms[i] : Math.abs(pNow[i]); pk = Math.max(pk, env[i], Math.abs(pNow[i])); }   // the scale holds the waveform's crest, not only its RMS
    for (let i = 0; i < n - 1; i++) upk = Math.max(upk, Math.abs(uNow[i]));
    S.pScale = Math.max(pk, S.pScale * 0.985, 1e-7);
    S.uScale = Math.max(upk, S.uScale * 0.985, 1e-7);
  }
  const X = (i) => x0 + (x1 - x0) * (n > 1 ? i / (n - 1) : 0);
  const smax = ins ? Math.max(...Array.from(ins.s.subarray(0, n)), 1e-6) : 1;
  const rad = (i) => R * (ins ? Math.sqrt(Math.max(ins.s[i], 1e-6) / smax) : 1);
  // the tube
  g.lineWidth = 2; g.strokeStyle = css('--tube'); g.lineCap = 'round';
  g.beginPath();
  if (n > 1) { for (let i = 0; i < n; i++) g.lineTo(X(i), yc - rad(i)); g.moveTo(X(0), yc + rad(0)); for (let i = 0; i < n; i++) g.lineTo(X(i), yc + rad(i)); }
  else { g.moveTo(x0, yc - R); g.lineTo(x1, yc - R); g.moveTo(x0, yc + R); g.lineTo(x1, yc + R); }
  g.stroke();
  const narrow = w < 560;
  g.fillStyle = css('--muted'); g.font = '11px system-ui, sans-serif'; g.textAlign = 'left';
  if (!narrow) g.fillText('embouchure end (the labium)', x0 + 4, yc + R + 16);
  g.textAlign = 'right'; g.fillText(narrow ? 'foot' : 'open foot', x1, yc + R + 16);
  if (!ins) {
    g.textAlign = 'center'; g.fillText(S.ctx ? 'Hold a note — the bore appears when it sounds.' : 'Start the sound, then hold a note.', (x0 + x1) / 2, yc + 4);
    drawJet(g, null, jetW, yc, R); return;
  }
  const k = 0.92 / S.pScale;
  // the envelope (the standing wave's shape) — symmetric, translucent
  g.fillStyle = css('--p-fill');
  g.beginPath(); for (let i = 0; i < n; i++) g.lineTo(X(i), yc - Math.min(1, env[i] * k) * rad(i));
  for (let i = n - 1; i >= 0; i--) g.lineTo(X(i), yc + Math.min(1, env[i] * k) * rad(i)); g.closePath(); g.fill();
  g.strokeStyle = css('--env'); g.lineWidth = 1.6;
  g.beginPath(); for (let i = 0; i < n; i++) g.lineTo(X(i), yc - Math.min(1, env[i] * k) * rad(i)); g.stroke();
  g.beginPath(); for (let i = 0; i < n; i++) g.lineTo(X(i), yc + Math.min(1, env[i] * k) * rad(i)); g.stroke();
  // the pressure now (every harmonic in it: a snapshot wanders inside the envelope)
  g.strokeStyle = css('--p'); g.lineWidth = 1.6;
  g.beginPath(); for (let i = 0; i < n; i++) g.lineTo(X(i), yc - Math.max(-1, Math.min(1, pNow[i] * k)) * rad(i)); g.stroke();
  // the flow (half nodes)
  const ku = 0.92 / S.uScale;
  g.strokeStyle = css('--u'); g.lineWidth = 1.2; g.setLineDash([4, 3]);
  g.beginPath(); for (let i = 0; i < n - 1; i++) g.lineTo(x0 + (x1 - x0) * ((i + 0.5) / (n - 1)), yc - Math.max(-1, Math.min(1, uNow[i] * ku)) * rad(i)); g.stroke();
  g.setLineDash([]);
  // the axis
  g.strokeStyle = css('--rule'); g.lineWidth = 1; g.beginPath(); g.moveTo(x0, yc); g.lineTo(x1, yc); g.stroke();
  g.fillStyle = css('--muted'); g.textAlign = 'right';
  g.fillText(`${n - 1} cells · |p| ≤ ${(S.pScale / P_REF).toFixed(2)} P_ref`, x1, 14);
  drawJet(g, ins, jetW, yc, R);
}

function drawJet(g, ins, jetW, yc, R) {
  const xf = 14, xe = jetW - 2, jw = Math.max(4, R * 0.11), narrow = jetW < 120;
  const y0 = ins ? ins.k[5] : 0.3;
  const yEdge = yc - y0 * jw;
  // the flue (a short channel) and the labium (a wedge pointing back at the flue)
  g.strokeStyle = css('--tube'); g.lineWidth = 2;
  g.beginPath(); g.moveTo(2, yc - jw * 1.2); g.lineTo(xf, yc - jw * 0.6); g.moveTo(2, yc + jw * 1.2); g.lineTo(xf, yc + jw * 0.6); g.stroke();
  g.fillStyle = css('--edge');
  g.beginPath(); g.moveTo(xe - 10, yEdge); g.lineTo(xe + 2, yEdge - 9); g.lineTo(xe + 2, yEdge + 9); g.closePath(); g.fill();
  g.fillStyle = css('--muted'); g.font = '11px system-ui, sans-serif'; g.textAlign = 'center';
  g.fillText('out', xe - 26, yEdge - 22); g.fillText(narrow ? 'in' : 'into the bore', xe - (narrow ? 22 : 40), yEdge + 30);
  g.fillText('flue', xf - 4, yc - jw * 1.6 - 4);
  if (!ins) return;
  // the jet: the delay line read along it (c, flue → labium), scaled to its displacement at the labium where the
  // engine applies the gain, compressed past three jet widths so the swing stays on the page
  const comp = (v) => 3 * Math.tanh(v / 3);
  const gain = ins.k[4];
  const pts = [];
  for (let i = 0; i < 64; i++) { const x = i / 63; const eta = -gain * ins.c[i] * x; pts.push([xf + (xe - 10 - xf) * x, yc - comp(eta) * jw]); }
  g.strokeStyle = css('--jet'); g.globalAlpha = 0.25; g.lineWidth = jw * 1.6; g.lineCap = 'round';
  g.beginPath(); for (const [x, y] of pts) g.lineTo(x, y); g.stroke();
  g.globalAlpha = 1; g.lineWidth = 2;
  g.beginPath(); for (const [x, y] of pts) g.lineTo(x, y); g.stroke();
  // the partition: the share of the jet entering the bore, ½(1 − tanh(η − y₀))
  const fin = 0.5 * (1 - Math.tanh(ins.k[9] - y0));
  const bx = 6, by = yc + R + 6, bw = jetW - 24;
  g.fillStyle = css('--rule'); g.fillRect(bx, by, bw, 5);
  g.fillStyle = css('--jet'); g.fillRect(bx, by, bw * fin, 5);
  g.fillStyle = css('--muted'); g.textAlign = 'left'; g.fillText(narrow ? `${Math.round(100 * fin)} % in` : `${Math.round(100 * fin)} % of the jet in`, bx, by + 18);
}

const phaseBuf = document.createElement('canvas');
function drawPhase() {
  const { g, w, h } = fit($('phase'));
  const s = Math.min(w, h), dpr = Math.min(2, window.devicePixelRatio || 1);
  if (phaseBuf.width !== Math.round(w * dpr) || phaseBuf.height !== Math.round(h * dpr)) { phaseBuf.width = Math.round(w * dpr); phaseBuf.height = Math.round(h * dpr); }
  const b = phaseBuf.getContext('2d'); b.setTransform(dpr, 0, 0, dpr, 0, 0);
  b.globalCompositeOperation = 'destination-out'; b.fillStyle = 'rgba(0,0,0,0.10)'; b.fillRect(0, 0, w, h);   // the persistence
  b.globalCompositeOperation = 'source-over';
  const cx = w / 2, cy = h / 2, sc = s * 0.42;
  // the sound against its slope, (s, ṡ/ω) — the flute's orbit trace is this very pair (DECISIONS_7 #24), read here from
  // the audio at the device's rate: the last two periods, so the portrait is the orbit and not an alias across many
  if (timeBuf && S.held && S.ctx) {
    const sr = S.ctx.sampleRate, f = S.hz > 0 ? S.hz : noteHz(S.note), w0 = 2 * Math.PI * f / sr;
    const len = Math.min(timeBuf.length - 2, Math.round(2 * sr / f) + 1), start = timeBuf.length - 1 - len;
    let pk = 1e-9; for (let i = start; i < start + len; i++) { const x = timeBuf[i], d = (timeBuf[i + 1] - timeBuf[i - 1]) / (2 * w0); pk = Math.max(pk, Math.hypot(x, d)); }
    S.phaseScale = Math.max(pk, (S.phaseScale || pk) * 0.97);
    b.strokeStyle = css('--trace'); b.lineWidth = 1.5; b.lineJoin = 'round'; b.beginPath();
    for (let i = start; i < start + len; i++) { const x = timeBuf[i] / S.phaseScale, d = (timeBuf[i + 1] - timeBuf[i - 1]) / (2 * w0) / S.phaseScale; b.lineTo(cx + x * sc, cy - d * sc); }
    b.stroke();
  }
  g.clearRect(0, 0, w, h);
  g.strokeStyle = css('--rule'); g.lineWidth = 1; g.beginPath(); g.moveTo(cx - sc, cy); g.lineTo(cx + sc, cy); g.moveTo(cx, cy - sc); g.lineTo(cx, cy + sc); g.stroke();
  g.drawImage(phaseBuf, 0, 0, w, h);
  g.fillStyle = css('--muted'); g.font = '11px system-ui, sans-serif'; g.textAlign = 'right'; g.fillText('s', cx + sc, cy - 4); g.textAlign = 'left'; g.fillText('ṡ / ω', cx + 4, cy - sc + 10);
}

let freqBuf = null;
function drawSpectrum() {
  const { g, w, h } = fit($('spectrum'));
  g.clearRect(0, 0, w, h);
  const sr = S.ctx ? S.ctx.sampleRate : 48000, fLo = 50, fHi = Math.min(12000, sr / 2);
  const X = (f) => 8 + (w - 16) * Math.log(f / fLo) / Math.log(fHi / fLo);
  const dbLo = -110, dbHi = -10, Y = (db) => 8 + (h - 24) * (1 - (Math.max(dbLo, Math.min(dbHi, db)) - dbLo) / (dbHi - dbLo));
  // the note's harmonics
  const f0 = noteHz(S.note);
  g.font = '10px system-ui, sans-serif'; g.textAlign = 'center';
  let lastLabel = -1e9;
  for (let k = 1; k * f0 < fHi && k <= 16; k++) {
    const x = X(k * f0); g.strokeStyle = css('--harm'); g.setLineDash([3, 4]); g.beginPath(); g.moveTo(x, 8); g.lineTo(x, h - 16); g.stroke();
    g.setLineDash([]); if (x - lastLabel >= 16) { g.fillStyle = css('--muted'); g.fillText(String(k), x, h - 4); lastLabel = x; }
  }
  if (S.analyser) {
    if (!freqBuf || freqBuf.length !== S.analyser.frequencyBinCount) freqBuf = new Float32Array(S.analyser.frequencyBinCount);
    S.analyser.getFloatFrequencyData(freqBuf);
    const binHz = sr / S.analyser.fftSize;
    g.beginPath(); g.moveTo(X(fLo), h - 16);
    for (let i = Math.ceil(fLo / binHz); i < freqBuf.length && i * binHz <= fHi; i++) g.lineTo(X(i * binHz), Y(freqBuf[i]));
    g.lineTo(X(fHi), h - 16); g.closePath();
    g.fillStyle = css('--p-fill'); g.fill(); g.strokeStyle = css('--p'); g.lineWidth = 1.2; g.stroke();
  }
  if (S.hz > 0) { const x = X(S.hz); g.strokeStyle = css('--accent'); g.lineWidth = 2; g.beginPath(); g.moveTo(x, 8); g.lineTo(x, h - 16); g.stroke(); }
}

let timeBuf = null, frame = 0;
function readPitch() {
  if (!S.analyser) return;
  if (!timeBuf) timeBuf = new Float32Array(S.analyser.fftSize);
  S.analyser.getFloatTimeDomainData(timeBuf);
  S.hz = S.held ? periodHz(timeBuf.subarray(timeBuf.length - 3072), S.ctx.sampleRate) : 0;
}
function drawReadouts() {
  const f0 = noteHz(S.note), ins = S.ins;
  $('r-note').textContent = `${noteName(S.note)} · ${f0.toFixed(1)} Hz`;
  if (S.hz > 0) {
    const reg = Math.max(1, Math.round(S.hz / f0));
    $('r-hz').textContent = `${S.hz.toFixed(1)} Hz`;
    $('r-reg').textContent = reg === 1 ? 'the first' : reg === 2 ? 'the octave' : reg === 3 ? 'the twelfth' : `${reg}×`;
    const c = 1200 * Math.log2(S.hz / (reg * f0)); $('r-cents').textContent = `${c >= 0 ? '+' : '−'}${Math.abs(c).toFixed(0)} cents`;
  } else { $('r-hz').textContent = S.held ? 'silent' : '—'; $('r-reg').textContent = '—'; $('r-cents').textContent = '—'; }
  if (ins) {
    $('r-pm').textContent = `${(ins.k[1] / P_REF).toFixed(2)} P_ref`;
    $('r-tau').textContent = `${(ins.k[3] * ins.freq / ins.rate2).toFixed(2)} periods`;
  } else { $('r-pm').textContent = '—'; $('r-tau').textContent = '—'; }
  if (S.cost) $('foot-cost').textContent = `the worklet: ${(1000 * S.cost.mean).toFixed(0)} µs a quantum on average (${(100 * S.cost.mean / (128000 / (S.sampleRate || 48000))).toFixed(1)} % of its time)${S.cost.max !== null ? `, ${(1000 * S.cost.max).toFixed(0)} µs at worst` : ''}`;
}
function drawHistory() {
  const { g, w, h } = fit($('history'));
  g.clearRect(0, 0, w, h);
  const now = performance.now() / 1000, span = 12, f0 = noteHz(S.note);
  S.history.push({ t: now, cents: S.hz > 0 ? 1200 * Math.log2(S.hz / f0) : null, breath: S.breath, held: S.held });
  while (S.history.length && S.history[0].t < now - span) S.history.shift();
  const cLo = -500, cHi = 2100, Y = (c) => 4 + (h - 8) * (1 - (c - cLo) / (cHi - cLo)), X = (t) => w * (1 - (now - t) / span);
  g.font = '10px system-ui, sans-serif'; g.textAlign = 'left';
  for (const [c, label] of [[0, 'the note'], [1200, 'octave'], [1902, 'twelfth']]) {
    g.strokeStyle = css('--rule'); g.lineWidth = 1; g.beginPath(); g.moveTo(0, Y(c)); g.lineTo(w, Y(c)); g.stroke();
    g.fillStyle = css('--muted'); g.fillText(label, 4, Y(c) - 3);
  }
  g.strokeStyle = css('--breath'); g.lineWidth = 1; g.globalAlpha = 0.7; g.beginPath();
  for (const p of S.history) g.lineTo(X(p.t), 4 + (h - 8) * (1 - p.breath / 127)); g.stroke(); g.globalAlpha = 1;
  g.strokeStyle = css('--trace'); g.lineWidth = 2; g.beginPath(); let pen = false;
  for (const p of S.history) { if (p.cents === null) { pen = false; continue; } const x = X(p.t), y = Y(Math.max(cLo, Math.min(cHi, p.cents))); if (pen) g.lineTo(x, y); else g.moveTo(x, y); pen = true; }
  g.stroke();
}

function tick() {
  frame++;
  readPitch();
  drawBore(); drawPhase(); drawSpectrum(); drawReadouts(); drawHistory();
  requestAnimationFrame(tick);
}

function wire() {
  buildKeys();
  $('power').addEventListener('click', power);
  $('volume').addEventListener('input', (e) => { if (S.gain) S.gain.gain.value = +e.target.value; });
  $('breath').addEventListener('input', (e) => { stopRamp(); sendBreath(+e.target.value); });
  $('ramp').addEventListener('click', () => { if (!S.ctx) power().then(startRamp); else startRamp(); });
  $('stop').addEventListener('click', release);
  for (const p of PARAMS) { const el = $(`p-${p}`); el.addEventListener('input', () => onParam(p, el)); }
  $('webmidi').addEventListener('change', (e) => webMidi(e.target.checked));
  requestAnimationFrame(tick);
  if (DEMO !== null) power();
}

if (GATE === 'overblow') runGate(); else wire();
