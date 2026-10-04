#!/usr/bin/env node
// suzu_lab_gate.mjs — the Suzu lab in a real browser (Phase 8 steps 59b–59c, DECISIONS_7 #29, #31).
//
//   node tools/suzu_lab_gate.mjs --dist build-web/suzu-dist --out <dir>
//        [--pages flute,cell,modal,strings,chaos,winds] [--chrome <path>] [--headed]
//        [--shots flute:70,cell,winds:light,strings:phone,…] [--settle 4000] [--timeout 120]
//
// Serves the lab and opens each page's browser check (<page>.html?gate=<name>) in Chrome
// (headless by default): the page renders its scripted demonstration OFFLINE through the
// same AudioWorklet and engine its live panel plays, measures the physics with its own
// code — the pitch it hears, the orbit it draws, the ledger it shows — and POSTs its checks
// ({ page, checks: [{ name, pass, detail }], data }). GREEN when every page answers and every
// check passes. Then --shots captures live pages (<page>.html?demo=<arg>) through the
// DevTools protocol after they have played a few seconds; a spec is page[:arg][:light][:phone].
// Exit: 0 green, 1 red, 4 a page gave no result, 2 usage.
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
const GATES = { flute: 'overblow', cell: 'cell', modal: 'bow', strings: 'strings', chaos: 'chaos', winds: 'winds' };
const pages = opt('--pages', Object.keys(GATES).join(',')).split(',').filter(Boolean);
const shots = opt('--shots', null);
const settle = Number(opt('--settle', '4000'));
const timeoutS = Number(opt('--timeout', '120'));
if (!fs.existsSync(path.join(dist, 'suzu.wasm'))) { console.error(`no suzu.wasm in ${dist}`); process.exit(2); }
fs.mkdirSync(out, { recursive: true });

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.wasm': 'application/wasm', '.css': 'text/css', '.png': 'image/png' };
let result = null;
const server = http.createServer((req, res) => {
  if (req.method === 'POST' && req.url === '/result') {
    let body = ''; req.on('data', (c) => { body += c; }); req.on('end', () => { try { result = JSON.parse(body); } catch { result = { checks: [{ name: 'a parseable result', pass: false, detail: 'unparseable' }] }; } res.writeHead(204); res.end(); });
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
let allGreen = true, missing = false;
const data = {};

for (const page of pages) {
  result = null;
  const child = spawn(chrome, [...flags, `http://127.0.0.1:${port}/${page}.html?gate=${GATES[page]}`], { stdio: ['ignore', 'pipe', 'pipe'] });
  let err = ''; child.stderr.on('data', (d) => { err += d; });
  const t0 = Date.now();
  while (!result && Date.now() - t0 < timeoutS * 1000) await new Promise((r) => setTimeout(r, 250));
  child.kill('SIGTERM');
  await new Promise((r) => setTimeout(r, 400));
  if (!result) { say(`[suzu-lab] ${page}: no result (timed out)`); fs.writeFileSync(path.join(out, `chrome_stderr_${page}.txt`), err); missing = true; allGreen = false; continue; }
  const pass = result.checks.every((c) => c.pass);
  if (!pass) allGreen = false;
  say(`[suzu-lab] ${page} (${result.userAgent?.match(/Chrome\/[\d.]+/)?.[0] ?? 'the browser'}) — ${pass ? 'GREEN' : 'RED'}`);
  for (const c of result.checks) say(`  ${c.pass ? 'ok  ' : 'FAIL'} ${c.name}${c.detail ? ` — ${c.detail}` : ''}`);
  if (result.data?.perSecond) say(`  second by second: ${result.data.perSecond}`);
  data[page] = result.data;
}
fs.writeFileSync(path.join(out, 'suzu_lab_data.json'), JSON.stringify(data, null, 1));
say(`[suzu-lab] ${allGreen ? 'GREEN' : 'RED'} — ${pages.length} page(s)`);

// ---- the screenshots of the live pages ----
if (shots) {
  const port2 = 9333 + Math.floor(Math.random() * 100);
  for (const spec of shots.split(',')) {
    const [page, ...rest] = spec.split(':');
    const mods = rest.filter((m) => m === 'light' || m === 'phone'), arg = rest.find((m) => m !== 'light' && m !== 'phone') ?? '';
    const url = `http://127.0.0.1:${port}/${page}.html?demo=${arg}`;
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
      const f = path.join(out, `lab_${page}${arg ? '_' + arg : ''}${mods.map((m) => '_' + m).join('')}.png`); fs.writeFileSync(f, Buffer.from(shot.data, 'base64'));
      say(`  screenshot: ${path.basename(f)} (${page}.html?demo=${arg}${mods.length ? ', ' + mods.join(', ') : ''}, after ${settle} ms)`);
    } catch (e) { say(`  screenshot ${spec}: FAILED — ${e.message}`); }
    child.kill('SIGTERM');
    await new Promise((r) => setTimeout(r, 500));
  }
}
fs.writeFileSync(path.join(out, 'suzu_lab_gate.txt'), lines.join('\n') + '\n');
server.close();
process.exit(missing ? 4 : allGreen ? 0 : 1);
