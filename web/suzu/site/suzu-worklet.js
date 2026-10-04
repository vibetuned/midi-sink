// suzu-worklet.js — the Suzu lab's audio thread (Phase 8 step 59b, DECISIONS_7 #28).
//
// The AudioWorkletProcessor owns the engine: it renders every 128-frame quantum with
// Voxo's own voxo_render, takes the page's MIDI and parameter messages between quanta,
// and posts a snapshot — the orbit trace and the inspection — about sixty times a
// second. Messages only, no SharedArrayBuffer: a static host (GitHub Pages) serves it
// without cross-origin isolation headers. processorOptions: { wasm (the bytes),
// params {name: value}, traceMask, segments, snapHz, envelope (default on), envelopeS (0.25),
// script [[block, status, d1, d2], …] }
// — the script is the gate's: MIDI at fixed quanta, deterministic in an OfflineAudioContext.
import { SuzuEngine, smoothAlong } from './suzu-engine.js';

const FINE_CLOCK = !!(globalThis.performance && performance.now);   // an AudioWorkletGlobalScope may have only Date (a millisecond)
const nowMs = () => (FINE_CLOCK ? performance.now() : Date.now());

class SuzuProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const o = options.processorOptions || {};
    this.engine = new SuzuEngine(new WebAssembly.Module(o.wasm), sampleRate, 16);
    if (o.params) for (const [k, v] of Object.entries(o.params)) this.engine.set(k, v);
    const ok = this.engine.apply();
    this.engine.traceMask(o.traceMask ?? 0x1ff);
    this.segments = o.segments ?? 12;
    this.every = Math.max(1, Math.round(sampleRate / 128 / (o.snapHz || 60)));
    this.script = o.script || null; this.si = 0;
    this.block = 0;
    this.muted = 0;                       // quanta to output silence for (after a patch change's calibration)
    this.cost = { sum: 0, n: 0, max: 0 };
    // THE ENVELOPE: the first sounding voice's state array `a` (the bore's pressure for the winds), averaged along the
    // bore (smoothAlong: the grid's shortest waves out), as an RMS per node,
    // accumulated every quantum — the page's sixty snapshots a second catch random phases of a field whose odd
    // harmonics sit 13 dB under the fundamental; 375 a second averaged over a quarter second show the mode's shape
    this.envOn = o.envelope !== false;
    this.env = new Float64Array(257); this.envN = 0; this.envSerial = -1; this.envTmp = new Float32Array(257);
    this.envA = 1 - Math.exp(-128 / (sampleRate * (o.envelopeS || 0.25)));
    this.port.onmessage = (e) => this.onMessage(e.data);
    this.port.postMessage({ type: 'ready', version: this.engine.version().text, sampleRate, applied: ok, log: this.engine.takeLog(), params: this.engine.params() });
  }

  onMessage(m) {
    const e = this.engine;
    switch (m.type) {
      case 'midi': for (const ev of m.events) e.midi(ev[0], ev[1], ev[2]); break;
      case 'params': {
        for (const [k, v] of Object.entries(m.values)) e.set(k, v);
        const t0 = nowMs(); const ok = e.apply(); const ms = nowMs() - t0;
        if (ms > 2) this.muted = 2;       // a calibration ran on this thread: the quanta it took were late — come back in silence
        this.port.postMessage({ type: 'applied', ok, ms, log: e.takeLog(), params: e.params() });
      } break;
      case 'gain': e.gain(m.value); break;
      case 'traceMask': e.traceMask(m.value); break;
      case 'segments': this.segments = m.value; break;
    }
  }

  process(inputs, outputs) {
    const out = outputs[0];
    const L = out[0], R = out[1] || out[0], frames = L.length;
    if (this.script) {
      while (this.si < this.script.length && this.script[this.si][0] <= this.block) { const ev = this.script[this.si++]; this.engine.midi(ev[1], ev[2], ev[3]); }
    }
    const t0 = nowMs();
    const buf = this.engine.render(frames);
    const dt = nowMs() - t0;
    this.cost.sum += dt; this.cost.n++; if (dt > this.cost.max) this.cost.max = dt;
    if (this.muted > 0) { this.muted--; L.fill(0); if (R !== L) R.fill(0); }
    else { for (let i = 0; i < frames; i++) { L[i] = buf[2 * i]; R[i] = buf[2 * i + 1]; } }
    if (this.envOn) {
      const v = this.engine.inspectView(), st = this.engine.inspectStride;
      if (v.length >= st) {
        const n = v[8], serial = v[4];
        if (n !== this.envN || serial !== this.envSerial) { this.env.fill(0); this.envN = n; this.envSerial = serial; }
        const a = this.envA, p = smoothAlong(v.subarray(16, 16 + n), n, this.envTmp);   // the drawing's average along the bore (suzu-engine.js)
        for (let i = 0; i < n; i++) this.env[i] += a * (p[i] * p[i] - this.env[i]);
      } else if (this.envN) { this.env.fill(0); this.envN = 0; this.envSerial = -1; }
    }
    this.block++;
    if (this.block % this.every === 0) {
      const cost = this.cost.n ? { mean: this.cost.sum / this.cost.n, max: FINE_CLOCK ? this.cost.max : null, quanta: this.cost.n, fine: FINE_CLOCK } : null;   // with a millisecond clock the mean of many quanta holds, a single quantum's worst does not
      if (this.cost.n >= 375) this.cost = { sum: 0, n: 0, max: 0 };   // a fresh window about every second
      const log = this.engine.takeLog();
      const env = this.envN ? Float32Array.from(this.env.subarray(0, this.envN), Math.sqrt) : null;
      this.port.postMessage({ type: 'snap', block: this.block, trace: this.engine.trace(this.segments), inspect: this.engine.inspect(), env, cost, log });
    }
    return true;
  }
}

registerProcessor('suzu', SuzuProcessor);
