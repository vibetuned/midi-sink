// lab-core.js — what every Suzu lab page shares (Phase 8 step 59c, DECISIONS_7 #31).
//
// The pages hold no physics: the AudioWorklet (suzu-worklet.js) runs the engine — Voxo's Suzu
// compiled to WebAssembly — and posts what the panels draw. Here: the audio graph and the
// MIDI a controller would send, the keyboard, the pitch read from the sound, the spectrum, a
// portrait with persistence, a strip chart, the red controls' banner, and the offline runner
// each page's browser check (?gate=…) renders its scripted demonstration with.
import { KIND_NAMES, smoothAlong } from './suzu-engine.js';
export { KIND_NAMES, smoothAlong };

export const QS = new URLSearchParams(location.search);
export const $ = (id) => document.getElementById(id);
export const css = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();
const NAMES = ['C', 'C♯', 'D', 'E♭', 'E', 'F', 'F♯', 'G', 'A♭', 'A', 'B♭', 'B'];
export const noteName = (m) => `${NAMES[((m % 12) + 12) % 12]}${Math.floor(m / 12) - 1}`;
export const noteHz = (m) => 440 * Math.pow(2, (m - 69) / 12);
export const clamp = (x, a, b) => Math.max(a, Math.min(b, x));

export const PAGES = [
  { id: 'cell', href: 'cell.html', title: 'The cell and the shears', blurb: 'One rotation in phase space: an orbit that keeps its size for ever, and the shears that comb harmonics into it.' },
  { id: 'modal', href: 'modal.html', title: 'The modal voice and the bow', blurb: 'A lattice of coupled cells — strings, bars, bells, glass — and a bow that holds each mode at the breath\'s energy.' },
  { id: 'strings', href: 'strings.html', title: 'The strings', blurb: 'A chain of masses, a delay line with a bridge, a modal pluck: three strings from three kinds of mathematics, side by side.' },
  { id: 'chaos', href: 'chaos.html', title: 'Duffing and the kicked rotor', blurb: 'A stiffening spring that clangs, and the standard map as an oscillator — order, then chaos, by one knob.' },
  { id: 'flute', href: 'flute.html', title: 'The flute', blurb: 'A bore and a jet with no moving parts: blow harder and it overblows to the octave by itself.' },
  { id: 'winds', href: 'winds.html', title: 'The saxophone and the trumpet', blurb: 'A reed on a cone, lips on a flared bore — and the ledger that says the mouth paid for every joule.' },
];
// The documentation site's root from the lab: the lab deploys beside /marble/ at the docs step (the documentation-timing
// rule), one level under the root; served alone (build-web/suzu-dist) the docs links lead nowhere.
export const DOCS_ROOT = '../';
// The site's navigation: every page's header carries <nav id="lab-nav">.
export function navigation(current) {
  const nav = $('lab-nav'); if (!nav) return;
  nav.innerHTML = `<a href="index.html"${current === 'index' ? ' aria-current="page"' : ''}>The lab</a>` +
    PAGES.map((p) => `<a href="${p.href}"${p.id === current ? ' aria-current="page"' : ''}>${p.title.replace(/^The /, '')}</a>`).join('');
}

// ---- the pitch, read from the sound: the autocorrelation's first lag within 0.08 of its maximum, refined by a
// parabola (the measure the desktop's calibration and gates use, DECISIONS_7 #21) ----
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
      return rate / (l + clamp(off, -0.5, 0.5));
    }
  }
  return 0;
}

// ---- the lab: the worklet, the analyser, the MIDI ----
const CH = 1;   // the MPE member channel the pages play on (the master is 0; the zone's 15 members, the bend range 48)
export class Lab {
  // opts: { params {name: value}, traceMask, traceDecim, recentPoints, envelope, snapHz, segments }
  constructor(opts) {
    this.opts = opts; this.ctx = null; this.node = null; this.analyser = null; this.gainNode = null;
    this.handlers = new Map(); this.ready = null; this.held = false; this.note = 60; this.velocity = 100;
    this.params = {}; this.version = ''; this.sampleRate = 0; this.timeBuf = null; this.cost = null;
    this.pending = new Map(); this.tag = 0; this.starting = null;
  }
  on(type, fn) { if (!this.handlers.has(type)) this.handlers.set(type, []); this.handlers.get(type).push(fn); return this; }
  emit(type, m) { for (const fn of this.handlers.get(type) || []) fn(m); }
  get running() { return !!this.ctx && this.ctx.state === 'running'; }

  async start() {
    if (this.ctx) { if (this.ctx.state !== 'running') await this.ctx.resume(); return; }
    if (this.starting) return this.starting;
    this.starting = (async () => {
      const wasm = await (await fetch('suzu.wasm')).arrayBuffer();
      this.ctx = new AudioContext({ latencyHint: 'interactive' });
      await this.ctx.audioWorklet.addModule('suzu-worklet.js');
      const o = this.opts;
      this.node = new AudioWorkletNode(this.ctx, 'suzu', { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2],
        processorOptions: { wasm, params: o.params, traceMask: o.traceMask ?? 0, traceDecim: o.traceDecim ?? 8, recentPoints: o.recentPoints ?? 0,
          envelope: o.envelope ?? false, segments: o.segments ?? 8, snapHz: o.snapHz ?? 60 } });
      this.ready = new Promise((res) => this.on('ready', res));
      this.node.port.onmessage = (e) => this.onMessage(e.data);
      this.gainNode = this.ctx.createGain(); this.gainNode.gain.value = 0.8;
      this.analyser = this.ctx.createAnalyser(); this.analyser.fftSize = 8192; this.analyser.smoothingTimeConstant = 0.55;
      this.node.connect(this.gainNode).connect(this.ctx.destination);
      this.node.connect(this.analyser);
      this.timeBuf = new Float32Array(this.analyser.fftSize);
      this.sampleRate = this.ctx.sampleRate;
      this.send([[0xB0, 101, 0], [0xB0, 100, 6], [0xB0, 6, 15]]);   // the MPE zone
      await this.ctx.resume();
      await this.ready;
    })();
    return this.starting;
  }
  async toggle() { if (!this.ctx) return this.start(); if (this.ctx.state === 'running') await this.ctx.suspend(); else await this.ctx.resume(); }

  onMessage(m) {
    if (m.type === 'ready') { this.version = m.version; this.params = m.params; }
    else if (m.type === 'snap') { if (m.cost) this.cost = m.cost; }
    else if (m.type === 'applied') { this.params = m.params; const p = this.pending.get(m.tag); if (p) { this.pending.delete(m.tag); p(m); } }
    this.emit(m.type, m);
  }

  send(events) { if (this.node) this.node.port.postMessage({ type: 'midi', events }); }
  // Applies parameters; resolves with { ok, log, ms } once the engine has (a rejected patch: ok false, the gate's sentence in log).
  setParams(values) {
    if (!this.node) { Object.assign(this.opts.params, values); return Promise.resolve({ ok: true, log: '', ms: 0 }); }
    const tag = ++this.tag;
    return new Promise((res) => { this.pending.set(tag, res); this.node.port.postMessage({ type: 'params', values, tag }); });
  }
  setTrace(t) { if (this.node) this.node.port.postMessage({ type: 'trace', ...t }); }
  reset() { if (this.node) this.node.port.postMessage({ type: 'reset' }); }
  gain(v) { if (this.gainNode) this.gainNode.gain.value = v; }

  hold(note, velocity = this.velocity) {
    const ev = [];
    if (this.held) ev.push([0x80 | CH, this.note, 0]);
    this.note = note; this.held = true;
    ev.push([0xE0 | CH, 0, 64], [0x90 | CH, note, velocity]);   // the bend centred first
    this.send(ev);
  }
  release() { if (this.held) this.send([[0x80 | CH, this.note, 0]]); this.held = false; }
  // A struck note: on, then off after `ms` (the release lets it ring as the patch says).
  strike(note, velocity = this.velocity, ms = 80) { this.hold(note, velocity); setTimeout(() => { if (this.held && this.note === note) this.release(); }, ms); }
  cc(n, v, ch = CH) { this.send([[0xB0 | ch, n, clamp(Math.round(v), 0, 127)]]); }
  pressure(v) { this.send([[0xD0 | CH, clamp(Math.round(v), 0, 127), 0]]); }
  bend(semitones) { const v = clamp(Math.round(8192 + semitones / 48 * 8191), 0, 16383); this.send([[0xE0 | CH, v & 127, v >> 7]]); }

  // The sounding pitch now (Hz, 0 when silent or held off) — and the time buffer the audio portraits read.
  readAudio() { if (!this.analyser) return; this.analyser.getFloatTimeDomainData(this.timeBuf); }
  pitch() { if (!this.analyser || !this.held) return 0; return periodHz(this.timeBuf.subarray(this.timeBuf.length - 3072), this.sampleRate); }
}

// The power button: start, then pause and resume.
export function bindPower(lab, button, onStarted) {
  button.addEventListener('click', async () => {
    if (!lab.ctx) {
      button.disabled = true; button.textContent = 'Starting…';
      try { await lab.start(); button.textContent = 'Pause the sound'; if (onStarted) onStarted(); }
      catch (err) { button.textContent = 'Start the sound'; showNote(`The sound could not start: ${err.message}. The lab needs a browser with AudioWorklet and WebAssembly.`); }
      button.disabled = false; return;
    }
    await lab.toggle(); button.textContent = lab.running ? 'Pause the sound' : 'Resume the sound';
  });
}
export async function ensureStarted(lab) { if (!lab.ctx) { $('power').disabled = true; await lab.start(); $('power').disabled = false; $('power').textContent = 'Pause the sound'; } }
export function showNote(text) { const el = $('hint'); if (el) el.textContent = text; }

// ---- the keyboard ----
export function buildKeys(container, { lo, hi, onDown }) {
  container.innerHTML = '';
  const black = (m) => [1, 3, 6, 8, 10].includes(((m % 12) + 12) % 12);
  const whites = []; for (let m = lo; m <= hi; m++) if (!black(m)) whites.push(m);
  const W = whites.length;
  for (let m = lo; m <= hi; m++) {
    const k = document.createElement('div');
    k.className = 'key' + (black(m) ? ' black' : ''); k.dataset.note = m;
    k.textContent = m % 12 === 0 ? noteName(m) : '';
    k.title = `${noteName(m)} · ${noteHz(m).toFixed(1)} Hz`;
    if (black(m)) { const i = whites.indexOf(m - 1); k.style.left = `${((i + 1) / W - 0.3 / W) * 100}%`; k.style.width = `${(0.6 / W) * 100}%`; }
    k.addEventListener('pointerdown', (e) => { e.preventDefault(); onDown(m, e); });
    container.appendChild(k);
  }
}
export function markKeys(container, note, on) { for (const k of container.querySelectorAll('.key')) k.classList.toggle('on', on && +k.dataset.note === note); }
// The computer's keyboard: A W S E D F T G Y H U J K from `base`.
export function bindKeymap(base, onNote) {
  const map = 'awsedftgyhujk';
  window.addEventListener('keydown', (e) => {
    if (e.repeat || e.metaKey || e.ctrlKey || e.altKey || ['INPUT', 'SELECT', 'TEXTAREA'].includes(e.target.tagName)) return;
    const i = map.indexOf(e.key.toLowerCase()); if (i >= 0) onNote(base + i);
  });
}

// ---- drawing ----
export function fit(cv) {
  const r = cv.getBoundingClientRect(), dpr = Math.min(2, window.devicePixelRatio || 1);
  const w = Math.max(1, Math.round(r.width * dpr)), h = Math.max(1, Math.round(r.height * dpr));
  if (cv.width !== w || cv.height !== h) { cv.width = w; cv.height = h; }
  const g = cv.getContext('2d'); g.setTransform(dpr, 0, 0, dpr, 0, 0);
  return { g, w: r.width, h: r.height, dpr };
}

let freqBuf = null;
// The spectrum, log frequency; dashed markers at k·f0 (the note's harmonics, or `ratios` for inharmonic modes).
export function drawSpectrum(cv, lab, { f0, ratios = null, sounding = 0, fLo = 40, fHi = 12000 }) {
  const { g, w, h } = fit(cv);
  g.clearRect(0, 0, w, h);
  const sr = lab.sampleRate || 48000; fHi = Math.min(fHi, sr / 2);
  const X = (f) => 8 + (w - 16) * Math.log(f / fLo) / Math.log(fHi / fLo);
  const dbLo = -110, dbHi = -10, Y = (db) => 8 + (h - 24) * (1 - (clamp(db, dbLo, dbHi) - dbLo) / (dbHi - dbLo));
  g.font = '10px system-ui, sans-serif'; g.textAlign = 'center';
  const marks = ratios ? ratios.map((r, i) => [r * f0, String(i + 1)]) : Array.from({ length: 16 }, (_, k) => [(k + 1) * f0, String(k + 1)]);
  let last = -1e9;
  for (const [f, label] of marks) {
    if (!(f > fLo && f < fHi)) continue;
    const x = X(f); g.strokeStyle = css('--harm'); g.setLineDash([3, 4]); g.beginPath(); g.moveTo(x, 8); g.lineTo(x, h - 16); g.stroke();
    g.setLineDash([]); if (x - last >= 16) { g.fillStyle = css('--muted'); g.fillText(label, x, h - 4); last = x; }
  }
  if (lab.analyser) {
    if (!freqBuf || freqBuf.length !== lab.analyser.frequencyBinCount) freqBuf = new Float32Array(lab.analyser.frequencyBinCount);
    lab.analyser.getFloatFrequencyData(freqBuf);
    const binHz = sr / lab.analyser.fftSize;
    g.beginPath(); g.moveTo(X(fLo), h - 16);
    for (let i = Math.ceil(fLo / binHz); i < freqBuf.length && i * binHz <= fHi; i++) g.lineTo(X(i * binHz), Y(freqBuf[i]));
    g.lineTo(X(fHi), h - 16); g.closePath();
    g.fillStyle = css('--p-fill'); g.fill(); g.strokeStyle = css('--p'); g.lineWidth = 1.2; g.stroke();
  }
  if (sounding > 0) { const x = X(sounding); g.strokeStyle = css('--accent'); g.lineWidth = 2; g.beginPath(); g.moveTo(x, 8); g.lineTo(x, h - 16); g.stroke(); }
}

// A portrait with persistence: each frame fades what was drawn and adds the new polyline(s).
export class Portrait {
  constructor(cv) { this.cv = cv; this.buf = document.createElement('canvas'); this.scale = 0; }
  // lines: arrays of [x, y] in the portrait's units; autoscale follows their extent (slow decay), or `fixed` holds it
  draw(lines, { fade = 0.12, fixed = 0, xLabel = 'x', yLabel = 'y', color = null, dots = false, guide = null } = {}) {
    const { g, w, h, dpr } = fit(this.cv);
    if (this.buf.width !== Math.round(w * dpr) || this.buf.height !== Math.round(h * dpr)) { this.buf.width = Math.round(w * dpr); this.buf.height = Math.round(h * dpr); }
    const b = this.buf.getContext('2d'); b.setTransform(dpr, 0, 0, dpr, 0, 0);
    b.globalCompositeOperation = 'destination-out'; b.fillStyle = `rgba(0,0,0,${fade})`; b.fillRect(0, 0, w, h);
    b.globalCompositeOperation = 'source-over';
    let ext = 1e-12; for (const l of lines) for (const [x, y] of l) ext = Math.max(ext, Math.abs(x), Math.abs(y));
    this.scale = fixed > 0 ? fixed : Math.max(ext, this.scale * 0.97);
    const cx = w / 2, cy = h / 2, sc = Math.min(w, h) * 0.44 / this.scale;
    b.strokeStyle = color || css('--trace'); b.fillStyle = color || css('--trace'); b.lineWidth = 1.5; b.lineJoin = 'round';
    for (const l of lines) {
      if (dots) { for (const [x, y] of l) b.fillRect(cx + x * sc - 1, cy - y * sc - 1, 2, 2); }
      else { b.beginPath(); for (const [x, y] of l) b.lineTo(cx + x * sc, cy - y * sc); b.stroke(); }
    }
    g.clearRect(0, 0, w, h);
    g.strokeStyle = css('--rule'); g.lineWidth = 1; g.beginPath(); g.moveTo(cx - Math.min(w, h) * 0.46, cy); g.lineTo(cx + Math.min(w, h) * 0.46, cy); g.moveTo(cx, cy - Math.min(w, h) * 0.46); g.lineTo(cx, cy + Math.min(w, h) * 0.46); g.stroke();
    if (guide) { g.strokeStyle = css('--env'); g.setLineDash([4, 4]); g.beginPath(); g.arc(cx, cy, guide * sc, 0, 2 * Math.PI); g.stroke(); g.setLineDash([]); }
    g.drawImage(this.buf, 0, 0, w, h);
    g.fillStyle = css('--muted'); g.font = '11px system-ui, sans-serif'; g.textAlign = 'right'; g.fillText(xLabel, cx + Math.min(w, h) * 0.46, cy - 4);
    g.textAlign = 'left'; g.fillText(yLabel, cx + 4, cy - Math.min(w, h) * 0.46 + 10);
    return sc;
  }
  clear() { const b = this.buf.getContext('2d'); b.clearRect(0, 0, this.buf.width, this.buf.height); }
}

// A strip chart over the last `span` seconds: series { key, color, lo, hi, label }, horizontal grid lines [value, label, seriesKey].
export class Strip {
  constructor(cv, span = 12) { this.cv = cv; this.span = span; this.rows = []; }
  // the time last: a series keyed `t` cannot overwrite it
  push(row) { const now = performance.now() / 1000; this.rows.push({ ...row, t: now }); while (this.rows.length && this.rows[0].t < now - this.span) this.rows.shift(); }
  draw(series, grid = []) {
    const { g, w, h } = fit(this.cv);
    g.clearRect(0, 0, w, h);
    const now = performance.now() / 1000, X = (t) => w * (1 - (now - t) / this.span);
    g.font = '10px system-ui, sans-serif'; g.textAlign = 'left';
    for (const [v, label, key] of grid) {
      const s = series.find((q) => q.key === key) || series[0]; const y = 4 + (h - 8) * (1 - (v - s.lo) / (s.hi - s.lo));
      g.strokeStyle = css('--rule'); g.lineWidth = 1; g.beginPath(); g.moveTo(0, y); g.lineTo(w, y); g.stroke();
      g.fillStyle = css('--muted'); g.fillText(label, 4, y - 3);
    }
    for (const s of series) {
      g.strokeStyle = s.color; g.lineWidth = s.width || 1.6; g.globalAlpha = s.alpha ?? 1; g.beginPath(); let pen = false;
      for (const r of this.rows) {
        const v = r[s.key]; if (v === null || v === undefined || !Number.isFinite(v)) { pen = false; continue; }
        const y = 4 + (h - 8) * (1 - (clamp(v, s.lo, s.hi) - s.lo) / (s.hi - s.lo));
        if (pen) g.lineTo(X(r.t), y); else g.moveTo(X(r.t), y); pen = true;
      }
      g.stroke(); g.globalAlpha = 1;
    }
  }
}

// ---- the red controls' banner: the gate's sentence, or the blow-up ----
export function banner(kind, text) {
  const el = $('banner'); if (!el) return;
  if (!text) { el.hidden = true; el.textContent = ''; return; }
  el.hidden = false; el.className = `banner ${kind}`; el.textContent = text;
}

// The footer's engine line and the worklet's cost.
export function footer(lab, kindName) {
  if (lab.version) $('foot-engine').textContent = `Voxo ${lab.version} in WebAssembly — Suzu's ${kindName}, no audio library: the worklet renders`;
  if (lab.cost) $('foot-cost').textContent = lab.cost.mean > 0 || lab.cost.fine
    ? `the worklet: ${(1000 * lab.cost.mean).toFixed(0)} µs a quantum on average (${(100 * lab.cost.mean / (128000 / (lab.sampleRate || 48000))).toFixed(1)} % of its time)${lab.cost.max !== null ? `, ${(1000 * lab.cost.max).toFixed(0)} µs at worst` : ''}`
    : 'the worklet: a quantum quicker than its millisecond clock can see';
}

// ---- the browser checks: a scripted demonstration rendered offline through the same worklet ----
export const MCM = [[0, 0xB0, 101, 0], [0, 0xB0, 100, 6], [0, 0xB0, 6, 15]];
export const at = (sr, s) => Math.round(s * sr / 128);   // seconds → the quantum
// opts: { params, script, seconds, sampleRate (48000), traceMask, traceDecim, recentPoints, envelope, snapHz }
// → { x (channel 0), sr, snaps [], ready, blowups [], restarts [] }
export async function offlineRun(opts) {
  const sr = opts.sampleRate || 48000;
  const wasm = await (await fetch('suzu.wasm')).arrayBuffer();
  const off = new OfflineAudioContext(2, Math.ceil(sr * opts.seconds), sr);
  await off.audioWorklet.addModule('suzu-worklet.js');
  const out = { sr, snaps: [], ready: null, blowups: [], restarts: [] };
  const node = new AudioWorkletNode(off, 'suzu', { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2],
    processorOptions: { wasm, params: opts.params, traceMask: opts.traceMask ?? 0, traceDecim: opts.traceDecim ?? 8, recentPoints: opts.recentPoints ?? 0,
      envelope: opts.envelope ?? false, segments: 8, snapHz: opts.snapHz ?? 30, script: [...MCM, ...(opts.script || [])].sort((a, b) => a[0] - b[0]) } });
  node.port.onmessage = (e) => {
    const m = e.data;
    if (m.type === 'snap') out.snaps.push(m); else if (m.type === 'ready') out.ready = m;
    else if (m.type === 'blowup') out.blowups.push(m); else if (m.type === 'restarted') out.restarts.push(m);
  };
  node.connect(off.destination);
  const t0 = performance.now();
  const buf = await off.startRendering();
  out.renderMs = performance.now() - t0;
  await new Promise((r) => setTimeout(r, 150));   // the worklet's last messages land
  out.x = buf.getChannelData(0);
  return out;
}
// The pitch of a rendered window ending at `t` seconds (4096 samples).
export function pitchAt(x, sr, t, len = 4096) { const end = Math.min(x.length, Math.floor(t * sr)); return periodHz(x.subarray(Math.max(0, end - len), end), sr); }
// Posts a page's checks to the gate tool: { page, checks: [{ name, pass, detail }], data }.
export async function postGate(result) {
  const line = document.createElement('p'); line.id = 'gate-line'; line.style.padding = '8px 16px';
  line.textContent = `gate ${result.page}: ${result.checks.filter((c) => c.pass).length} of ${result.checks.length} checks pass`;
  document.body.prepend(line);
  await fetch('/result', { method: 'POST', body: JSON.stringify({ ...result, userAgent: navigator.userAgent }) }).catch(() => {});
}
export async function gateGuard(page, fn) {
  try { await postGate({ page, ...(await fn()) }); }
  catch (err) { await postGate({ page, checks: [{ name: 'the page ran its check', pass: false, detail: String(err && err.stack || err) }], data: null }); }
}

// ---- the bore (the flute, the sax, the trumpet): the engine's pressure node by node inside the tube drawn from its
// area, the flow dashed, the envelope (the worklet's RMS per node over a quarter second, drawn as √2·RMS — a sine's
// peak); all drawn through smoothAlong, which keeps the harmonics and leaves out the grid's own shortest waves.
// `drawEnd(g, ins, endW, yc, R, narrow)` draws the exciter at the blown end (the jet, the reed, the lips).
export class BoreView {
  constructor(cv, { pRef = 0.005, leftLabel = 'the blown end', rightLabel = 'the open end' } = {}) {
    this.cv = cv; this.pRef = pRef; this.leftLabel = leftLabel; this.rightLabel = rightLabel; this.pScale = 1e-4; this.uScale = 1e-4;
    // THE CONE: its pressure grows toward the apex as one over the distance, so drawn as it is the narrow end takes the
    // scale and the rest of the bore looks still; it is p·r that stands as a sine (the spherical wave's 1-D equation).
    // With `cone` set the pressure and its envelope are drawn times the local radius over the widest (r ∝ the distance
    // from the apex along a cone; the mouthpiece's cylinder keeps the tip's radius)
    this.cone = false;
  }
  draw(ins, envRms, drawEnd, idleText) {
    const { g, w, h } = fit(this.cv);
    g.clearRect(0, 0, w, h);
    const endW = Math.min(170, Math.max(90, w * 0.17)), x0 = endW, x1 = w - 18, yc = h * 0.52, R = h * 0.27, narrow = w < 560;
    const n = ins ? ins.n : 0;
    const env = new Float32Array(n);
    const pNow = ins ? smoothAlong(ins.a, n) : null, uNow = ins ? smoothAlong(ins.b, Math.max(0, n - 1)) : null;
    let smax = 1e-6; if (ins) for (let i = 0; i < n; i++) smax = Math.max(smax, ins.s[i]);
    const rn = (i) => Math.sqrt(Math.max(ins.s[i], 1e-6) / smax);
    if (ins) {
      let pk = 0, upk = 0;
      for (let i = 0; i < n; i++) {
        env[i] = envRms && envRms.length === n ? Math.SQRT2 * envRms[i] : Math.abs(pNow[i]);
        if (this.cone) { const r = rn(i); env[i] *= r; pNow[i] *= r; }
        pk = Math.max(pk, env[i], Math.abs(pNow[i]));   // the scale holds the waveform's crest, not only its RMS
      }
      for (let i = 0; i < n - 1; i++) upk = Math.max(upk, Math.abs(uNow[i]));
      this.pScale = Math.max(pk, this.pScale * 0.985, 1e-7);
      this.uScale = Math.max(upk, this.uScale * 0.985, 1e-7);
    }
    const X = (i) => x0 + (x1 - x0) * (n > 1 ? i / (n - 1) : 0);
    const rad = (i) => R * (ins ? rn(i) : 1);
    g.lineWidth = 2; g.strokeStyle = css('--tube'); g.lineCap = 'round';
    g.beginPath();
    if (n > 1) { for (let i = 0; i < n; i++) g.lineTo(X(i), yc - rad(i)); g.moveTo(X(0), yc + rad(0)); for (let i = 0; i < n; i++) g.lineTo(X(i), yc + rad(i)); }
    else { g.moveTo(x0, yc - R); g.lineTo(x1, yc - R); g.moveTo(x0, yc + R); g.lineTo(x1, yc + R); }
    g.stroke();
    g.fillStyle = css('--muted'); g.font = '11px system-ui, sans-serif'; g.textAlign = 'left';
    if (!narrow) g.fillText(this.leftLabel, x0 + 4, yc + R + 16);
    g.textAlign = 'right'; g.fillText(narrow ? this.rightLabel.split(' ').pop() : this.rightLabel, x1, yc + R + 16);
    if (!ins) { g.textAlign = 'center'; g.fillText(idleText, (x0 + x1) / 2, yc + 4); drawEnd(g, null, endW, yc, R, narrow); return; }
    const k = 0.92 / this.pScale;
    g.fillStyle = css('--p-fill');
    g.beginPath(); for (let i = 0; i < n; i++) g.lineTo(X(i), yc - Math.min(1, env[i] * k) * rad(i));
    for (let i = n - 1; i >= 0; i--) g.lineTo(X(i), yc + Math.min(1, env[i] * k) * rad(i)); g.closePath(); g.fill();
    g.strokeStyle = css('--env'); g.lineWidth = 1.6;
    g.beginPath(); for (let i = 0; i < n; i++) g.lineTo(X(i), yc - Math.min(1, env[i] * k) * rad(i)); g.stroke();
    g.beginPath(); for (let i = 0; i < n; i++) g.lineTo(X(i), yc + Math.min(1, env[i] * k) * rad(i)); g.stroke();
    g.strokeStyle = css('--p'); g.lineWidth = 1.6;
    g.beginPath(); for (let i = 0; i < n; i++) g.lineTo(X(i), yc - clamp(pNow[i] * k, -1, 1) * rad(i)); g.stroke();
    const ku = 0.92 / this.uScale;
    g.strokeStyle = css('--u'); g.lineWidth = 1.2; g.setLineDash([4, 3]);
    g.beginPath(); for (let i = 0; i < n - 1; i++) g.lineTo(x0 + (x1 - x0) * ((i + 0.5) / (n - 1)), yc - clamp(uNow[i] * ku, -1, 1) * rad(i)); g.stroke();
    g.setLineDash([]);
    g.strokeStyle = css('--rule'); g.lineWidth = 1; g.beginPath(); g.moveTo(x0, yc); g.lineTo(x1, yc); g.stroke();
    g.fillStyle = css('--muted'); g.textAlign = 'right';
    g.fillText(this.cone ? `${n - 1} cells · drawn: p × r / r_bell, ≤ ${(this.pScale / this.pRef).toFixed(2)} P_ref` : `${n - 1} cells · |p| ≤ ${(this.pScale / this.pRef).toFixed(2)} P_ref`, x1, 14);
    drawEnd(g, ins, endW, yc, R, narrow);
  }
}

// The sound's phase plane (s, ṡ/ω) from the audio: the last two periods at the device's rate.
export function audioOrbit(lab, f) {
  if (!lab.timeBuf || !lab.held || !(f > 0)) return [];
  const sr = lab.sampleRate, w0 = 2 * Math.PI * f / sr, tb = lab.timeBuf;
  const len = Math.min(tb.length - 2, Math.round(2 * sr / f) + 1), start = tb.length - 1 - len, pts = [];
  for (let i = start; i < start + len; i++) pts.push([tb[i], (tb[i + 1] - tb[i - 1]) / (2 * w0)]);
  return pts;
}
// The register readout: the nearest multiple of the note and the cents off it.
export function register(hz, f0) {
  if (!(hz > 0)) return null;
  const reg = Math.max(1, Math.round(hz / f0));
  return { reg, cents: 1200 * Math.log2(hz / (reg * f0)), name: reg === 1 ? 'the first' : reg === 2 ? 'the octave' : reg === 3 ? 'the twelfth' : reg === 4 ? 'two octaves' : `${reg}×` };
}
export const signed = (c, digits = 0) => `${c >= 0 ? '+' : '−'}${Math.abs(c).toFixed(digits)}`;

// The spectral peak near a frequency (±span cents, 2-cent steps, Hann-windowed Goertzel): the hybrid's string
// fundamental beside its body's own modes, where the waveform's period is the body's and the strings' together.
export function peakNear(x, sr, f, span = 100) {
  const n = x.length; let best = 0, bf = 0;
  for (let c = -span; c <= span; c += 2) {
    const fr = f * Math.pow(2, c / 1200), w = 2 * Math.PI * fr / sr, k = 2 * Math.cos(w); let s1 = 0, s2 = 0;
    for (let i = 0; i < n; i++) { const win = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / (n - 1)); const s0 = x[i] * win + k * s1 - s2; s2 = s1; s1 = s0; }
    const m = s1 * s1 + s2 * s2 - k * s1 * s2; if (m > best) { best = m; bf = fr; }
  }
  return { hz: bf, amp: Math.sqrt(Math.max(best, 0)) / n };
}
