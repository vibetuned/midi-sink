// suzu-engine.js — Suzu in the browser (Phase 8 step 59b, DECISIONS_7 #28).
//
// One engine, two hosts: the lab's AudioWorklet renders the sound with it, and the web
// gate (tools/suzu_web_gate.mjs) renders its script with it in node. It wraps the
// standalone suzu.wasm — Voxo with no device, the synth only — through suzu_web.c's flat
// surface: parameters by name, MIDI bytes in, interleaved stereo out, the orbit trace
// and the inspection as plain objects. Nothing here is DOM, fetch or timer: it runs in
// an AudioWorkletGlobalScope, where none of those exist.

const UTF8 = (bytes) => {   // the worklet scope has no TextDecoder: Voxo's log lines carry — × ∫ in UTF-8
  let s = '';
  for (let i = 0; i < bytes.length;) {
    const b = bytes[i++];
    let cp = b;
    if (b >= 0xf0) { cp = ((b & 7) << 18) | ((bytes[i++] & 63) << 12) | ((bytes[i++] & 63) << 6) | (bytes[i++] & 63); }
    else if (b >= 0xe0) { cp = ((b & 15) << 12) | ((bytes[i++] & 63) << 6) | (bytes[i++] & 63); }
    else if (b >= 0xc0) { cp = ((b & 31) << 6) | (bytes[i++] & 63); }
    s += String.fromCodePoint(cp);
  }
  return s;
};

// The drawing's average along a bore or a string: [1, 2, 1]/4 twice. It keeps the 8th harmonic of a 94-cell bore at
// 96 % and removes the grid's own shortest waves (near the grid's cutoff, slow by dispersion: the jet's switching
// drives them — 6 % of the flute bore's pressure energy at breath 70, 21 % at full breath — inaudible at the mouth
// end, loud in a snapshot). `out` may be `x`'s length; the ends are kept.
export function smoothAlong(x, n, out) {
  const a = out || new Float32Array(n);
  let prev, cur;
  for (let pass = 0; pass < 2; pass++) {
    const src = pass === 0 ? x : a;
    prev = src[0]; cur = src[0];
    for (let i = 0; i < n; i++) {
      const next = i + 1 < n ? src[i + 1] : src[i];
      cur = src[i];
      a[i] = i === 0 || i === n - 1 ? cur : 0.25 * prev + 0.5 * cur + 0.25 * next;
      prev = cur;
    }
  }
  return a;
}

export const KIND_NAMES = ['cell', 'lattice', 'Verlet string', 'hybrid string', 'Duffing', 'kicked rotor', 'flute', 'saxophone', 'trumpet'];
export const INSPECT_MAX = 257;
export const TRACE_POINTS_MAX = 17;

export class SuzuEngine {
  // `module` a WebAssembly.Module of suzu.wasm (compile the bytes once: new WebAssembly.Module(bytes))
  constructor(module, sampleRate, maxVoices = 16) {
    // the module imports nothing today; should a toolchain add an import, a stub answers 0
    const imports = {};
    for (const imp of WebAssembly.Module.imports(module)) {
      imports[imp.module] ??= {};
      if (imp.kind === 'function') imports[imp.module][imp.name] = () => 0;
    }
    const inst = new WebAssembly.Instance(module, imports);
    this.x = inst.exports;
    if (this.x._initialize) this.x._initialize();
    this.mem = this.x.memory;
    this.sampleRate = sampleRate;
    this.v = this.x.sw_create(sampleRate >>> 0, maxVoices >>> 0);
    if (!this.v) throw new Error('suzu: the engine could not be created');
    this.names = [];
    this.index = new Map();
    this.isInt = [];
    const n = this.x.sw_param_count();
    for (let i = 0; i < n; i++) {
      const name = this.cstr(this.x.sw_param_name(i));
      this.names.push(name); this.index.set(name, i); this.isInt.push(this.x.sw_param_is_int(i) !== 0);
    }
    this.outPtr = this.x.sw_out_ptr();
    this.traceStride = this.x.sw_trace_stride();
    this.inspectStride = this.x.sw_inspect_stride();
  }

  cstr(ptr) {
    const u8 = new Uint8Array(this.mem.buffer);
    let end = ptr; while (u8[end] !== 0) end++;
    return UTF8(u8.subarray(ptr, end));
  }

  version() { const v = this.x.sw_version(); return { major: v >>> 16, minor: (v >>> 8) & 255, patch: v & 255, text: `${v >>> 16}.${(v >>> 8) & 255}.${v & 255}` }; }

  // ---- the parameters (voxo_suzu_params_t by name) ----
  defaults() { this.x.sw_param_defaults(); }
  set(name, value) { const i = this.index.get(name); if (i === undefined) throw new Error(`suzu: no parameter "${name}"`); this.x.sw_param_set(i, +value); }
  get(name) { const i = this.index.get(name); return i === undefined ? undefined : this.x.sw_param_get(i); }
  params() { const o = {}; this.names.forEach((n, i) => { o[n] = this.x.sw_param_get(i); }); return o; }
  // Applies the staged parameters; false when Voxo rejects the patch (takeLog() says why). The sax and the
  // trumpet calibrate here (a fraction of a second): the worklet mutes while it runs.
  apply() { return this.x.sw_apply(this.v) !== 0; }

  takeLog() {
    const len = this.x.sw_log_len();
    if (!len) return '';
    const s = UTF8(new Uint8Array(this.mem.buffer, this.x.sw_log_ptr(), len));
    this.x.sw_log_clear();
    return s;
  }

  // ---- the play ----
  midi(status, d1 = 0, d2 = 0) { this.x.sw_midi(this.v, status & 255, d1 & 255, d2 & 255); }
  gain(g) { this.x.sw_set_gain(this.v, g); }
  // Renders `frames` (≤ 1024) and returns a VIEW of the interleaved stereo output (valid until the next render).
  render(frames = 128) { this.x.sw_render(this.v, frames); return new Float32Array(this.mem.buffer, this.outPtr, 2 * frames); }

  // ---- the orbit trace (voxo_trace_poll) ----
  traceMask(mask) { this.x.sw_trace_mask(this.v, mask >>> 0); }
  traceRaw(maxSegments = 8) {
    const n = this.x.sw_trace(this.v, maxSegments >>> 0);
    return new Float32Array(this.mem.buffer, this.x.sw_trace_ptr(), n * this.traceStride).slice();
  }
  trace(maxSegments = 8) {
    const raw = this.traceRaw(maxSegments), st = this.traceStride, out = [];
    for (let o = 0; o + st <= raw.length; o += st) {
      const count = raw[o];
      out.push({ count, amplitude: raw[o + 1], note: raw[o + 2], kind: raw[o + 3], held: raw[o + 4] !== 0, serial: raw[o + 5], channel: raw[o + 6],
        x: raw.slice(o + 8, o + 8 + count), y: raw.slice(o + 8 + TRACE_POINTS_MAX, o + 8 + TRACE_POINTS_MAX + count) });
    }
    return out;
  }

  // ---- the recent trace (step 59c: voxo_trace_recent — the ring's last points at full density) ----
  traceDecimation(subSteps) { this.x.sw_trace_decimation(this.v, subSteps >>> 0); }
  // A VIEW of up to `max` points (x, y, u, w interleaved; valid until the next call); recent() copies.
  recentView(voice = 0, max = 1023) { const n = this.x.sw_trace_recent(this.v, voice >>> 0, max >>> 0); return new Float32Array(this.mem.buffer, this.x.sw_recent_ptr(), 4 * n); }
  recent(voice = 0, max = 1023) { return this.recentView(voice, max).slice(); }

  // ---- the inspection (voxo_suzu_inspect; the arrays by kind are documented in voxo.h) ----
  inspectRaw() { return this.inspectView().slice(); }
  // A VIEW of the inspection's records (no copy, valid until the next inspect): the worklet's per-quantum reads.
  inspectView() {
    const n = this.x.sw_inspect(this.v);
    return new Float32Array(this.mem.buffer, this.x.sw_inspect_ptr(), n * this.inspectStride);
  }
  inspect() {
    const raw = this.inspectRaw(), st = this.inspectStride, M = INSPECT_MAX, out = [];
    for (let o = 0; o + st <= raw.length; o += st) {
      const n = raw[o + 8], kind = raw[o + 2];
      const at = (j, len) => raw.slice(o + 16 + j * M, o + 16 + j * M + len);
      out.push({ channel: raw[o], note: raw[o + 1], kind, held: raw[o + 3] !== 0, serial: raw[o + 4], freq: raw[o + 5], env: raw[o + 6], rate2: raw[o + 7], n,
        a: at(0, n), b: at(1, n), c: at(2, kind === 6 ? 64 : n), s: at(3, n), k: raw.slice(o + 16 + 4 * M, o + 16 + 4 * M + 16) });
    }
    return out;
  }

  destroy() { if (this.v) this.x.sw_destroy(this.v); this.v = 0; }
}
