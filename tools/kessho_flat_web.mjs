#!/usr/bin/env node
// kessho_flat_web.mjs — the Kessho flatness reading on the WEB TIER (v3.0 step 92; the desktop's is --kessho-flat,
// step 71, DECISIONS_10 #4): "z = 1 is flat for any exact field".
//
//   node tools/kessho_flat_web.mjs <field_webgpu.bin> [--bar 1e-3] [--margin 16]
//
// Reads a field dump written by the marble (tools/web_gate.mjs: ?fielddump=1 — the §4.6 canonical script of exact
// operators, the same .bin the desktop writes: a little-endian header w, h as uint32 × 2, then float32 RGBA rows,
// the payload dx, dy, ink, aux since libsumi 1.2.0) and computes |det J − 1| of the pre-image u = x + dx, v = y + dy
// by central differences over ±1, ±2 and ±3 texels — the desktop's t71_flatness estimator: the interior `margin`
// texels in, a stencil that straddles a drop's rim (the ink band's parity differs across it) masked — and prints the
// median, the 99th percentile, the maximum and the rms per stencil. One mask the desktop's soak scenes never needed:
// the canonical script lays INK drops, and a fresh drop's interior is new material whose every texel sources the
// drop's centre — the pre-image collapses to a point and det J is 0 by construction, not by error; a stencil whose
// five taps share one pre-image is masked as fresh ink and counted. The roadmap's bar is 1e-3 (the fp16 web tier);
// the desktop's record (DECISIONS_10 #4) is that the exact operators read the resampler's and the half-float field's
// FLOOR — a median of 2e-3 at ±3 texels to 5e-3 at ±1 — and the rims own the maximum. Exit 0 when the ±3 median is
// under the bar, 3 when it is not (the reading is the evidence either way), 2 on usage.
import fs from 'node:fs';

const args = process.argv.slice(2);
const file = args.find((a) => !a.startsWith('--'));
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? Number(args[i + 1]) : d; };
const bar = opt('--bar', 1e-3), margin = opt('--margin', 16);
if (!file || !fs.existsSync(file)) { console.error('usage: kessho_flat_web.mjs <field_webgpu.bin> [--bar 1e-3] [--margin 16]'); process.exit(2); }
const buf = fs.readFileSync(file);
const W = buf.readUInt32LE(0), H = buf.readUInt32LE(4);
const px = new Float32Array(buf.buffer, buf.byteOffset + 8, W * H * 4);
console.log(`[kessho-flat web] ${file}: ${W} × ${H}, the payload dx, dy, ink, aux (the pre-image u = x + dx, v = y + dy)`);

const at = (x, y) => (y * W + x) * 4;
const u = (x, y) => (x + 0.5) / W + px[at(x, y)], v = (x, y) => (y + 0.5) / H + px[at(x, y) + 1];
const det = (x, y, w) => {
  const sx = W / (2 * w), sy = H / (2 * w);
  const ux = (u(x + w, y) - u(x - w, y)) * sx, uy = (u(x, y + w) - u(x, y - w)) * sy;
  const vx = (v(x + w, y) - v(x - w, y)) * sx, vy = (v(x, y + w) - v(x, y - w)) * sy;
  return ux * vy - uy * vx;
};
const straddles = (x, y, w) => {
  const b0 = Math.floor(px[at(x, y) + 2]);
  return [[x + w, y], [x - w, y], [x, y + w], [x, y - w]].some(([i, j]) => Math.floor(px[at(i, j) + 2]) !== b0);
};
// fresh ink: the five taps' pre-images coincide (a drop's interior, sourced from its centre) — det J is 0 by construction
const fresh = (x, y, w) => {
  const u0 = u(x, y), v0 = v(x, y);
  return [[x + w, y], [x - w, y], [x, y + w], [x, y - w]].every(([i, j]) => Math.abs(u(i, j) - u0) < 1e-6 && Math.abs(v(i, j) - v0) < 1e-6);
};
// clamped: the pre-image is constant along one axis across the stencil — the sampler's CLAMP_TO_EDGE where an operator's
// tail reaches past the sheet (the border texel is returned for every lookup beyond it), det J 0 on that axis by the
// sampler, not by the operator; counted and masked, their bounding box printed (they belong at the edges)
const clamped = (x, y, w) => u(x + w, y) === u(x - w, y) || v(x, y + w) === v(x, y - w);
let verdict = 0;
for (const w of [1, 2, 3]) {
  const devs = []; let acc = 0, mx = 0, masked = 0, ink = 0, cl = 0, bx0 = W, bx1 = 0, by0 = H, by1 = 0;
  for (let y = margin; y + margin < H; y++) for (let x = margin; x + margin < W; x++) {
    if (straddles(x, y, w)) { masked++; continue; }
    if (fresh(x, y, w)) { ink++; continue; }
    if (clamped(x, y, w)) { cl++; bx0 = Math.min(bx0, x); bx1 = Math.max(bx1, x); by0 = Math.min(by0, y); by1 = Math.max(by1, y); continue; }
    const d = Math.abs(det(x, y, w) - 1); devs.push(d); acc += d * d; if (d > mx) mx = d;
  }
  devs.sort((a, b) => a - b);
  const n = devs.length, median = n ? devs[Math.floor(0.5 * (n - 1))] : 0, p99 = n ? devs[Math.floor(0.99 * (n - 1))] : 0, rms = n ? Math.sqrt(acc / n) : 0;
  console.log(`  ±${w} texel${w > 1 ? 's' : ''}: median ${median.toExponential(2)}  p99 ${p99.toExponential(2)}  max ${mx.toExponential(2)}  rms ${rms.toExponential(2)}  (${n} texels; ${masked} masked at the rims, ${ink} fresh ink, ${cl} clamped${cl ? ` in x ${bx0}–${bx1}, y ${by0}–${by1}` : ''})`);
  if (w === 3) verdict = median <= bar ? 0 : 3;
}
console.log(`[kessho-flat web] the ±3 median against the bar ${bar}: ${verdict === 0 ? 'UNDER — flat at the web tier' : 'OVER — the field\'s floor at this tier (the desktop reads a few thousandths too: DECISIONS_10 #4)'}`);
process.exit(verdict);
