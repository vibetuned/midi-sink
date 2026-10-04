#!/usr/bin/env node
// suzu_web_gate.mjs — the Suzu lab's web gate (Phase 8 step 59b, DECISIONS_7 #29).
//
//   node tools/suzu_web_gate.mjs --wasm build-web/suzu-dist/suzu.wasm
//        --ref build/tests/suzu_web_reference --ref-desktop build/tests/suzu_web_reference_desktop
//        --script tests/fixtures/suzu_web_script.txt --out <dir>
//
// Renders the script through the SAME engine module the lab's AudioWorklet runs
// (web/suzu/site/suzu-engine.js over suzu.wasm) and compares it record by record
// — the audio, the orbit trace, the inspection — against the native reference
// built from the same flat surface (web/suzu/suzu_web.c) over the no-FMA Voxo:
//   * BIT FOR BIT for every voice kind whose path calls no math-library function
//     whose last bit differs between the platforms' libraries;
//   * within a DECLARED TOLERANCE for the kinds that do — the lattice's sin per
//     sample under a glide, the flute's tanh, the trumpet's pow (TOLERANT below,
//     relative to each record's peak);
// then reports the wasm against the SHIPPING desktop build (fused multiply-add on)
// in dB — information, not a verdict: the chaotic rotor diverges there, as chaos
// does with a last-bit difference. NEGATIVE CONTROL: one sample of the reference
// flipped must turn the comparison red. Also measures the wasm's size and the
// engine's cost per 128-frame quantum per voice kind (node's V8, the browser's engine).
// Exit: 0 green, 1 red, 3 the negative control passed (the gate is broken), 2 usage.
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { performance } from 'node:perf_hooks';
import { SuzuEngine, KIND_NAMES } from '../web/suzu/site/suzu-engine.js';

const args = process.argv.slice(2);
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : d; };
const wasmPath = opt('--wasm', 'build-web/suzu-dist/suzu.wasm');
const refBin = opt('--ref', 'build/tests/suzu_web_reference');
const deskBin = opt('--ref-desktop', 'build/tests/suzu_web_reference_desktop');
const scriptPath = opt('--script', 'tests/fixtures/suzu_web_script.txt');
const out = path.resolve(opt('--out', 'gate-suzu-web'));
fs.mkdirSync(out, { recursive: true });
for (const f of [wasmPath, refBin, deskBin, scriptPath]) if (!fs.existsSync(f)) { console.error(`missing ${f}`); process.exit(2); }

// THE DECLARED TOLERANCE: the kinds whose path calls a math-library function whose last bit differs between
// Apple's libm and emscripten's musl for some arguments — the lattice's sinf per sample under a glide (its
// first difference falls on the script's bend), the flute's tanhf from the first block, the trumpet's powf.
// −80 dB of each record's peak (measured: −113, −92, −728); every other kind is bit for bit.
const TOLERANT = new Map([[1, 1e-4], [6, 1e-4], [8, 1e-4]]);

// ---- the engine on the script ----------------------------------------------------------------
const mod = new WebAssembly.Module(fs.readFileSync(wasmPath));
const num = (s) => Number(s.startsWith('0x') || s.startsWith('0X') ? parseInt(s, 16) : s);
function runScript() {
  const recs = []; const cost = new Map();
  let rate = 48000, eng = null, kind = 0, evs = [], snaps = [];
  for (const raw of fs.readFileSync(scriptPath, 'utf8').split('\n')) {
    const t = raw.trim().split(/\s+/); if (!t[0] || t[0].startsWith('#')) continue;
    switch (t[0]) {
      case 'rate': rate = num(t[1]); break;
      case 'create': if (eng) eng.destroy(); eng = new SuzuEngine(mod, rate, 16); eng.defaults(); break;
      case 'param': eng.set(t[1], Number(t[2])); if (t[1] === 'voice_kind') kind = Number(t[2]); break;
      case 'apply': if (!eng.apply()) throw new Error(`the patch was rejected (kind ${kind}): ${eng.takeLog()}`); break;
      case 'trace': eng.traceMask(num(t[1])); break;
      case 'midi': evs.push([num(t[1]), num(t[2]), num(t[3]), num(t[4])]); break;
      case 'snap': snaps.push(num(t[1])); break;
      case 'render': {
        const blocks = num(t[1]); const audio = new Float32Array(blocks * 256); let ei = 0; let ms = 0;
        for (let b = 0; b < blocks; b++) {
          for (; ei < evs.length && evs[ei][0] <= b; ei++) eng.midi(evs[ei][1], evs[ei][2], evs[ei][3]);
          const t0 = performance.now(); const o = eng.render(128); ms += performance.now() - t0;
          audio.set(o, b * 256);
          if (snaps.includes(b)) recs.push({ type: 2, kind, block: b, trace: eng.traceRaw(8), inspect: eng.inspectRaw() });
        }
        recs.push({ type: 1, kind, audio });
        cost.set(kind, 1000 * ms / blocks);
        evs = []; snaps = [];
      } break;
    }
  }
  if (eng) eng.destroy();
  return { recs, cost };
}

// ---- the reference's records -----------------------------------------------------------------
function readRecords(file) {
  const buf = fs.readFileSync(file); const f = new Float32Array(buf.buffer, buf.byteOffset, buf.byteLength / 4);
  const recs = []; let i = 0;
  while (i < f.length) {
    const type = f[i++], kind = f[i++];
    if (type === 1) { const n = f[i++]; recs.push({ type: 1, kind, audio: f.slice(i, i + n) }); i += n; }
    else if (type === 2) { const block = f[i++]; const nt = f[i++]; const trace = f.slice(i, i + nt); i += nt; const ni = f[i++]; const inspect = f.slice(i, i + ni); i += ni; recs.push({ type: 2, kind, block, trace, inspect }); }
    else throw new Error(`${file}: a corrupt record at float ${i}`);
  }
  return recs;
}
const runRef = (bin, name) => {
  const file = path.join(out, name);
  const r = spawnSync(path.resolve(bin), [path.resolve(scriptPath), file], { encoding: 'utf8' });
  if (r.status !== 0) { console.error(`${bin}: exit ${r.status}\n${r.stderr}`); process.exit(1); }
  return readRecords(file);
};

// ---- the comparison --------------------------------------------------------------------------
function compareArrays(a, b) {   // { same: fraction bit-identical, rel: max|diff| / peak, n }
  if (a.length !== b.length) return { same: 0, rel: Infinity, n: Math.max(a.length, b.length), lengthMismatch: true };
  const ua = new Uint32Array(a.buffer, a.byteOffset, a.length), ub = new Uint32Array(b.buffer, b.byteOffset, b.length);
  let same = 0, md = 0, pk = 0;
  for (let i = 0; i < a.length; i++) {
    if (ua[i] === ub[i]) same++;
    const d = Math.abs(a[i] - b[i]); if (!(d <= md)) md = d;   // NaN-safe: a NaN difference counts as infinite
    pk = Math.max(pk, Math.abs(b[i]));
  }
  return { same: a.length ? same / a.length : 1, rel: pk > 0 ? md / pk : md, n: a.length };
}
function compare(web, ref) {
  const perKind = new Map();
  const note = (kind, what, c) => { const k = perKind.get(kind) ?? { kind, parts: [] }; k.parts.push({ what, ...c }); perKind.set(kind, k); };
  if (web.length !== ref.length) return { ok: false, why: `${web.length} records against ${ref.length}`, perKind };
  let ok = true;
  for (let r = 0; r < web.length; r++) {
    const w = web[r], f = ref[r];
    if (w.type !== f.type || w.kind !== f.kind) return { ok: false, why: `record ${r}: type/kind ${w.type}/${w.kind} against ${f.type}/${f.kind}`, perKind };
    const tol = TOLERANT.get(w.kind) ?? 0;
    const parts = w.type === 1 ? [['audio', w.audio, f.audio]] : [[`trace@${w.block}`, w.trace, f.trace], [`inspect@${w.block}`, w.inspect, f.inspect]];
    for (const [what, a, b] of parts) {
      const c = compareArrays(a, b);
      const pass = !c.lengthMismatch && (tol === 0 ? c.same === 1 : c.rel <= tol);
      if (!pass) ok = false;
      note(w.kind, what, { ...c, pass, tol });
    }
  }
  return { ok, perKind };
}
const db = (rel) => rel > 0 ? 20 * Math.log10(rel) : -Infinity;

// ---- run ---------------------------------------------------------------------------------------
const lines = [];
const say = (s) => { lines.push(s); console.log(s); };
const wasmBytes = fs.statSync(wasmPath).size;
say(`[suzu-web] the Suzu lab's web gate — ${path.basename(wasmPath)} ${wasmBytes} bytes, script ${path.basename(scriptPath)}`);
const ref = runRef(refBin, 'ref_nofma.bin');
const desk = runRef(deskBin, 'ref_desktop.bin');
const { recs: web, cost } = runScript();
{   // the wasm's own records, for the archive
  const parts = [];
  for (const r of web) {
    if (r.type === 1) parts.push(Float32Array.of(1, r.kind, r.audio.length), r.audio);
    else parts.push(Float32Array.of(2, r.kind, r.block, r.trace.length), r.trace, Float32Array.of(r.inspect.length), r.inspect);
  }
  fs.writeFileSync(path.join(out, 'web.bin'), Buffer.concat(parts.map((p) => Buffer.from(p.buffer, p.byteOffset, p.byteLength))));
}

const main = compare(web, ref);
say(`the wasm against the native reference (no fused multiply-add) — ${main.ok ? 'GREEN' : 'RED'}${main.why ? ': ' + main.why : ''}`);
for (const [kind, k] of [...main.perKind.entries()].sort((a, b) => a[0] - b[0])) {
  const audio = k.parts.find((p) => p.what === 'audio');
  const snaps = k.parts.filter((p) => p.what !== 'audio');
  const tol = TOLERANT.get(kind) ?? 0;
  const snapSame = snaps.every((p) => p.same === 1), snapRel = Math.max(...snaps.map((p) => p.rel));
  say(`  ${String(kind)} ${KIND_NAMES[kind].padEnd(14)} audio ${(100 * audio.same).toFixed(2).padStart(6)} % bit-identical, max |diff| ${audio.rel === 0 ? 'none' : db(audio.rel).toFixed(1) + ' dB of the peak'}; trace and inspection ${snapSame ? 'bit-identical' : `within ${db(snapRel).toFixed(1)} dB`} — ${tol ? `the declared tolerance ${db(tol).toFixed(0)} dB (libm)` : 'bit for bit'}: ${k.parts.every((p) => p.pass) ? 'ok' : 'FAIL'}`);
}

const vsDesk = compare(web, desk);
say('the wasm against the SHIPPING desktop build (fused multiply-add on) — the report:');
for (const [kind, k] of [...vsDesk.perKind.entries()].sort((a, b) => a[0] - b[0])) {
  const audio = k.parts.find((p) => p.what === 'audio');
  say(`  ${String(kind)} ${KIND_NAMES[kind].padEnd(14)} max |diff| ${audio.rel === 0 ? 'none' : db(audio.rel).toFixed(1) + ' dB of the peak'}${kind === 5 ? ' (the chaotic rotor: a last-bit difference grows, as chaos does — the statistics, not the samples)' : ''}`);
}

// NEGATIVE CONTROL: one sample of the reference flipped in the first kind's audio
const corrupt = ref.map((r) => ({ ...r, audio: r.audio ? r.audio.slice() : undefined }));
const victim = corrupt.find((r) => r.type === 1 && r.kind === 0);
victim.audio[12345] = victim.audio[12345] + 0.25;
const neg = compare(web, corrupt);
say(`negative control (one reference sample of the cell moved by 0.25): ${neg.ok ? 'PASSED — the gate is broken' : 'red, as required'}`);

say('the engine\'s cost per 128-frame quantum in node (V8; two voices, 48 kHz, the budget 2667 µs):');
for (const [kind, us] of [...cost.entries()].sort((a, b) => a[0] - b[0])) say(`  ${String(kind)} ${KIND_NAMES[kind].padEnd(14)} ${us.toFixed(1).padStart(6)} µs  (${(100 * us / 2666.7).toFixed(2)} % of the quantum)`);

fs.writeFileSync(path.join(out, 'suzu_web_gate.txt'), lines.join('\n') + '\n');
if (neg.ok) process.exit(3);
process.exit(main.ok ? 0 : 1);
