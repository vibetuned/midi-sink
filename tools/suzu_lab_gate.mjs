#!/usr/bin/env node
// suzu_lab_gate.mjs — the Suzu lab in a real browser (Phase 8 step 59b, DECISIONS_7 #29).
//
//   node tools/suzu_lab_gate.mjs --dist build-web/suzu-dist --out <dir>
//        [--chrome "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"] [--headed]
//        [--shots 70,127,70:light,70:phone] [--settle 4000] [--timeout 90]
//
// Serves the lab, opens ?gate=overblow in Chrome (headless by default): the page renders
// a breath ramp — A4 held, the breath 0 → 127 over 12 s, then 3 s at the top — offline
// through the SAME AudioWorklet and engine the live page plays, reads the sounding pitch
// from the rendered audio with its own estimator every quarter second, and POSTs the
// rows. GREEN when: the audio is finite; mid-ramp the flute sounds its first register on
// the note (within 30 cents); the last second sounds the octave (within 60 cents) — the
// overblow, autonomous; and blowing softly flattens (the τ-phase lag). Then, with
// --shots, the live page (?demo=<breath>) is captured through the DevTools protocol after
// it has played a few seconds. Exit: 0 green, 1 red, 4 no result (the page or the audio
// failed), 2 usage.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';

const args = process.argv.slice(2);
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : d; };
const dist = path.resolve(opt('--dist', 'build-web/suzu-dist'));
const out = path.resolve(opt('--out', 'gate-suzu-lab'));
const chrome = opt('--chrome', process.platform === 'darwin' ? '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' : 'google-chrome');
const headed = args.includes('--headed');
const shots = opt('--shots', null);
const settle = Number(opt('--settle', '4000'));
const timeoutS = Number(opt('--timeout', '90'));
if (!fs.existsSync(path.join(dist, 'suzu.wasm'))) { console.error(`no suzu.wasm in ${dist}`); process.exit(2); }
fs.mkdirSync(out, { recursive: true });

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.wasm': 'application/wasm', '.css': 'text/css', '.png': 'image/png' };
let result = null;
const server = http.createServer((req, res) => {
  if (req.method === 'POST' && req.url === '/result') {
    let body = ''; req.on('data', (c) => { body += c; }); req.on('end', () => { try { result = JSON.parse(body); } catch { result = { error: 'unparseable result' }; } res.writeHead(204); res.end(); });
    return;
  }
  const url = new URL(req.url, 'http://x');
  const file = path.join(dist, url.pathname === '/' ? 'index.html' : url.pathname);
  if (!file.startsWith(dist) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream' });
  fs.createReadStream(file).pipe(res);
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const port = server.address().port;
const flags = [`--user-data-dir=${path.join(out, 'chrome-profile')}`, '--no-first-run', '--no-default-browser-check', '--window-size=1280,980',
  '--autoplay-policy=no-user-gesture-required'];
if (!headed) flags.unshift('--headless=new');

const lines = [];
const say = (s) => { lines.push(s); console.log(s); };

// ---- the gate ----
{
  const child = spawn(chrome, [...flags, `http://127.0.0.1:${port}/?gate=overblow`], { stdio: ['ignore', 'pipe', 'pipe'] });
  let err = ''; child.stderr.on('data', (d) => { err += d; });
  const t0 = Date.now();
  while (!result && Date.now() - t0 < timeoutS * 1000) await new Promise((r) => setTimeout(r, 250));
  child.kill('SIGTERM');
  fs.writeFileSync(path.join(out, 'chrome_stderr.txt'), err);
}
if (!result || result.error) {
  say(`[suzu-lab] no result from the page${result && result.error ? `: ${result.error}` : ' (timed out)'}`);
  fs.writeFileSync(path.join(out, 'suzu_lab_gate.txt'), lines.join('\n') + '\n');
  server.close(); process.exit(4);
}
fs.writeFileSync(path.join(out, 'overblow_rows.json'), JSON.stringify(result, null, 1));
const rows = result.rows;
const near = (b) => rows.filter((r) => Math.abs(r.breath - b) <= 3 && r.hz > 0);
const mean = (a) => a.reduce((s, x) => s + x, 0) / (a.length || 1);
const first = rows.filter((r) => r.breath >= 40 && r.breath <= 90 && r.register === 1);
const onNote = first.filter((r) => Math.abs(r.cents) <= 30);
const top = rows.filter((r) => r.t >= result.seconds - 1.0);
const octave = top.filter((r) => r.register === 2 && Math.abs(r.cents) <= 60);
const jump = rows.find((r) => r.register >= 2);
const soft = near(32), mid = near(56);
const flattens = soft.length && mid.length && mean(soft.map((r) => r.cents)) < mean(mid.map((r) => r.cents)) - 10;
say(`[suzu-lab] the flute in ${result.userAgent.match(/Chrome\/[\d.]+/)?.[0] ?? 'the browser'}: A4 held, the breath ramped 0 → 127 over 12 s then held — ${result.seconds} s rendered offline through the AudioWorklet in ${result.renderMs} ms`);
say(`  the audio finite: ${result.finite ? 'yes' : 'NO'}`);
say(`  mid-ramp (breath 40–90) in the first register: ${first.length} windows, ${onNote.length} within 30 cents of the note (closest ${first.length ? Math.min(...first.map((r) => Math.abs(r.cents))).toFixed(1) : '—'})`);
say(`  the last second at full breath: ${octave.length} of ${top.length} windows on the octave within 60 cents (${top.map((r) => `${r.register}×${r.cents === null ? '—' : (r.cents >= 0 ? '+' : '') + r.cents}`).join(' ')})`);
say(`  the jump: the first window above the first register at ${jump ? `t ${jump.t} s, breath ${jump.breath}/127 (${jump.register}×, ${jump.hz} Hz)` : 'none'}`);
say(`  soft blowing flattens: breath ~32 reads ${soft.length ? mean(soft.map((r) => r.cents)).toFixed(1) : '—'} cents, breath ~56 ${mid.length ? mean(mid.map((r) => r.cents)).toFixed(1) : '—'} — ${flattens ? 'yes' : 'NO'}`);
say('  the pitch second by second (breath: register×cents): ' + rows.filter((r) => Math.abs(r.t - Math.round(r.t)) < 0.01).map((r) => `${r.breath}:${r.register ? `${r.register}×${r.cents >= 0 ? '+' : ''}${r.cents}` : 'silent'}`).join('  '));
if (result.costs?.length) {
  const c = result.costs[result.costs.length - 1];
  say(`  the worklet's own cost (offline, ${c.quanta} quanta): ${(1000 * c.mean).toFixed(0)} µs a quantum on average${c.max !== null ? `, ${(1000 * c.max).toFixed(0)} µs at worst` : ' (the worklet has only a millisecond clock here: the mean of many quanta holds, a single quantum\'s worst would be the clock\'s tick)'}`);
}
const green = result.finite && onNote.length > 0 && octave.length >= Math.ceil(top.length / 2) && flattens;
say(`[suzu-lab] ${green ? 'GREEN' : 'RED'}`);

// ---- the screenshots of the live page ----
if (shots) {
  const port2 = 9333 + Math.floor(Math.random() * 100);
  for (const spec of shots.split(',')) {
    const [b, ...mods] = spec.split(':');   // a breath, then :light (the light theme) and/or :phone (390 px wide)
    const url = `http://127.0.0.1:${port}/?demo=${b}`;
    const child = spawn(chrome, [...flags, `--remote-debugging-port=${port2}`, url], { stdio: ['ignore', 'pipe', 'pipe'] });
    try {
      let target = null;
      for (let i = 0; i < 40 && !target; i++) {
        await new Promise((r) => setTimeout(r, 250));
        try { const list = await (await fetch(`http://127.0.0.1:${port2}/json/list`)).json(); target = list.find((t) => t.type === 'page' && t.url.startsWith(`http://127.0.0.1:${port}/`)); } catch {}
      }
      if (!target) throw new Error('no DevTools page target');
      const ws = new WebSocket(target.webSocketDebuggerUrl);
      await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
      let id = 0; const pending = new Map();
      ws.onmessage = (m) => { const d = JSON.parse(m.data); const p = pending.get(d.id); if (p) { pending.delete(d.id); d.result ? p.res(d.result) : p.rej(new Error(JSON.stringify(d.error))); } };
      const cdp = (method, params = {}) => new Promise((res, rej) => { const i = ++id; pending.set(i, { res, rej }); ws.send(JSON.stringify({ id: i, method, params })); });
      await cdp('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-color-scheme', value: mods.includes('light') ? 'light' : 'dark' }] });
      if (mods.includes('phone')) await cdp('Emulation.setDeviceMetricsOverride', { width: 390, height: 844, deviceScaleFactor: 2, mobile: true });
      await new Promise((r) => setTimeout(r, settle));
      const shot = await cdp('Page.captureScreenshot', { format: 'png', captureBeyondViewport: true });
      ws.close();
      const f = path.join(out, `lab_flute_breath${b}${mods.map((m) => '_' + m).join('')}.png`); fs.writeFileSync(f, Buffer.from(shot.data, 'base64')); say(`  screenshot: ${path.basename(f)} (?demo=${b}${mods.length ? ', ' + mods.join(', ') : ''}, after ${settle} ms)`);
    } catch (e) { say(`  screenshot ${spec}: FAILED — ${e.message}`); }
    child.kill('SIGTERM');
    await new Promise((r) => setTimeout(r, 500));
  }
}
fs.writeFileSync(path.join(out, 'suzu_lab_gate.txt'), lines.join('\n') + '\n');
server.close();
process.exit(green ? 0 : 1);
