// suzu-worklet.js — the Suzu lab's audio thread (Phase 8 steps 59b–59c, DECISIONS_7 #28, #31).
//
// The AudioWorkletProcessor owns the engine: it renders every 128-frame quantum with
// Voxo's own voxo_render, takes the page's MIDI and parameter messages between quanta,
// and posts a snapshot — the orbit trace, the inspection, the ring's recent points at
// full density, the bore's envelope — about sixty times a second. Messages only, no
// SharedArrayBuffer: a static host (GitHub Pages) serves it without cross-origin
// isolation headers.
//
// processorOptions: { wasm (the bytes), params {name: value}, traceMask, traceDecim (the
// ring's density, sub-steps a point), recentPoints (the most a snapshot carries, 0 = none),
// segments, snapHz, envelope (default on), envelopeS (0.25), script [[block, status, d1, d2], …] }
// — the script is the gates': MIDI at fixed quanta, deterministic in an OfflineAudioContext.
//
// THE SAFETY (59c, the red controls): Voxo clips its own mix at full scale, so a runaway voice
// does not leave the range — it sits at ±1, a full-scale wave, while its state grows to 10¹²
// and beyond (found: the naive cell). So a quantum that reaches the clip (|x| ≥ 0.98: the lab's
// voices sit under 0.5) goes SILENT at once, and a non-finite sample, or twelve such quanta, is
// a BLOW-UP: the page is told, and the engine restarts from the staged parameters (a voice
// whose state went non-finite never ends by itself). The red controls exist to show the gates'
// reasons; nothing here lets them reach the speakers.
import { SuzuEngine, smoothAlong } from './suzu-engine.js';

const FINE_CLOCK = !!(globalThis.performance && performance.now);   // an AudioWorkletGlobalScope may have only Date (a millisecond)
const nowMs = () => (FINE_CLOCK ? performance.now() : Date.now());

class SuzuProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const o = options.processorOptions || {};
    this.engine = new SuzuEngine(new WebAssembly.Module(o.wasm), sampleRate, 16);
    this.traceMask = o.traceMask ?? 0x1ff;
    this.traceDecim = o.traceDecim ?? 8;
    if (o.params) for (const [k, v] of Object.entries(o.params)) this.engine.set(k, v);
    const ok = this.engine.apply();
    this.configureTrace();
    this.segments = o.segments ?? 12;
    this.recentPoints = o.recentPoints ?? 0;
    this.every = Math.max(1, Math.round(sampleRate / 128 / (o.snapHz || 60)));
    this.script = o.script || null; this.si = 0;
    this.block = 0; this.lastSnapBlock = 0;
    this.muted = 0;                       // quanta to output silence for (after a slow apply, after a blow-up)
    this.blowups = 0; this.hot = 0;       // the blow-ups so far; the quanta in a row at the clip
    this.cost = { sum: 0, n: 0, max: 0 };
    // THE ENVELOPE: the first sounding voice's state array `a` (the bore's pressure for the winds), averaged along the
    // bore (smoothAlong: the grid's shortest waves out), as an RMS per node, accumulated every quantum — the page's
    // sixty snapshots a second catch random phases of a field whose odd harmonics sit 13 dB under the fundamental
    this.envOn = o.envelope !== false;
    this.env = new Float64Array(257); this.envN = 0; this.envSerial = -1; this.envTmp = new Float32Array(257);
    this.envA = 1 - Math.exp(-128 / (sampleRate * (o.envelopeS || 0.25)));
    this.port.onmessage = (e) => this.onMessage(e.data);
    this.port.postMessage({ type: 'ready', version: this.engine.version().text, sampleRate, applied: ok, log: this.engine.takeLog(), params: this.engine.params() });
  }

  configureTrace() { this.engine.traceMask(this.traceMask); this.engine.traceDecimation(this.traceDecim); }

  onMessage(m) {
    const e = this.engine;
    switch (m.type) {
      case 'midi': for (const ev of m.events) e.midi(ev[0], ev[1], ev[2]); break;
      case 'params': {
        for (const [k, v] of Object.entries(m.values)) e.set(k, v);
        const t0 = nowMs(); const ok = e.apply(); const ms = nowMs() - t0;
        if (ms > 2) this.muted = Math.max(this.muted, 2);   // a calibration ran on this thread: its quanta were late — come back in silence
        this.envN = 0; this.envSerial = -1;
        this.port.postMessage({ type: 'applied', ok, ms, log: e.takeLog(), params: e.params(), tag: m.tag });
      } break;
      case 'gain': e.gain(m.value); break;
      case 'trace': if (m.mask !== undefined) this.traceMask = m.mask; if (m.decim !== undefined) this.traceDecim = m.decim; this.configureTrace(); break;
      case 'segments': this.segments = m.value; break;
      case 'reset': this.restart('the page asked'); break;
    }
  }

  // A fresh instance from the staged parameters (voices, rings and state cleared), the trace as it was.
  restart(why) {
    this.engine.destroy();
    this.engine.v = this.engine.x.sw_create(sampleRate >>> 0, 16);
    const ok = this.engine.apply();
    this.configureTrace();
    this.envN = 0; this.envSerial = -1;
    this.port.postMessage({ type: 'restarted', why, ok, log: this.engine.takeLog() });
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
    let blown = false, atClip = false;
    for (let i = 0; i < 2 * frames; i++) { const x = buf[i]; if (!(x > -0.98 && x < 0.98)) { atClip = true; if (!Number.isFinite(x)) { blown = true; break; } } }   // NaN fails both comparisons
    this.hot = atClip ? this.hot + 1 : 0;
    if (this.hot >= 12) blown = true;
    if (blown || atClip || this.muted > 0) {
      if (this.muted > 0) this.muted--;
      L.fill(0); if (R !== L) R.fill(0);
    } else {
      for (let i = 0; i < frames; i++) { L[i] = Math.max(-1, Math.min(1, buf[2 * i])); R[i] = Math.max(-1, Math.min(1, buf[2 * i + 1])); }
    }
    if (blown) {
      this.blowups++;
      const ins = this.engine.inspect();   // what the state looked like as it went
      this.port.postMessage({ type: 'blowup', block: this.block, count: this.blowups, inspect: ins });
      this.restart('a blow-up');
      this.muted = 8; this.hot = 0;
    } else if (this.envOn) {
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
      const cost = this.cost.n ? { mean: this.cost.sum / this.cost.n, max: FINE_CLOCK ? this.cost.max : null, quanta: this.cost.n, fine: FINE_CLOCK } : null;   // with a millisecond clock the mean of many quanta holds, one quantum's worst does not
      if (this.cost.n >= 375) this.cost = { sum: 0, n: 0, max: 0 };   // a fresh window about every second
      const log = this.engine.takeLog();
      const env = this.envN ? Float32Array.from(this.env.subarray(0, this.envN), Math.sqrt) : null;
      // the recent points since the last snapshot (the ring holds 1023): the portraits and the sections read every one
      let recent = null;
      if (this.recentPoints > 0) {
        const fresh = Math.ceil((this.block - this.lastSnapBlock) * frames * 2 / this.traceDecim);
        recent = this.engine.recent(0, Math.min(this.recentPoints, Math.max(fresh, 2)));
      }
      this.lastSnapBlock = this.block;
      this.port.postMessage({ type: 'snap', block: this.block, trace: this.engine.trace(this.segments), inspect: this.engine.inspect(), env, recent, cost, log });
    }
    return true;
  }
}

registerProcessor('suzu', SuzuProcessor);
