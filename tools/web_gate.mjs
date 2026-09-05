#!/usr/bin/env node
// web_gate.mjs — the §4.6 WEB TIER field regression, end to end (Phase 5 §5).
//
//   node tools/web_gate.mjs --dist build-web/web-dist --out gate-web
//        [--chrome "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"]
//        [--headed] [--compare build/tests/field_dump_compare
//         --fixture tests/fixtures/field_512_metal.bin --max-tol 2.5e-2 --mean-tol 1e-3]
//        [--timeout 60]
//
// Serves the wasm build, opens ?fielddump=1 in Chrome (headless by default),
// and waits for the page to POST the field dump (/field) — the same .bin the
// desktop harness writes — plus its console (/log). Then, if a comparator is
// given, compares against the Metal fixture with the web tier's tolerance.
// Exit: 0 green, 1 regression, 4 no dump (renderer/WebGPU unavailable), 2 usage.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { spawn, spawnSync } from 'node:child_process';

const args = process.argv.slice(2);
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : d; };
const has = (k) => args.includes(k);
const dist = path.resolve(opt('--dist', 'build-web/web-dist'));
const out = path.resolve(opt('--out', 'gate-web'));
const chrome = opt('--chrome', process.platform === 'darwin'
  ? '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' : 'google-chrome');
const compare = opt('--compare', null);
const fixture = opt('--fixture', null);
const maxTol = opt('--max-tol', '1e-2');
const meanTol = opt('--mean-tol', '1e-4');
const timeoutS = Number(opt('--timeout', '60'));
const headed = has('--headed');
const scenesMode = has('--scenes');   // sweep every operator scene via query params
const noUnsafe = has('--no-unsafe');   // stock Chrome: no --enable-unsafe-webgpu (what a user's tab gets)
const shotScenes = opt('--shots', null); // comma list: capture each scene's canvas after 90 frames
const fullShots = opt('--fullshots', null); // comma list of query strings (e.g. "scene=wake,") or page paths ("/guide/"): full-page PNG via DevTools
const windowSize = opt('--window', '900,700'); // WxH of the headless window for the captures
fs.mkdirSync(out, { recursive: true });

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.wasm': 'application/wasm', '.css': 'text/css' };
const logLines = [];
let dumpPath = null;
let sceneDone = null;
let shotBuf = null;

const server = http.createServer((req, res) => {
  if (req.method === 'POST' && req.url === '/log') {
    let body = ''; req.on('data', (c) => body += c); req.on('end', () => { logLines.push(body); res.writeHead(204); res.end(); });
    return;
  }
  if (req.method === 'POST' && req.url === '/scene') {
    let body = ''; req.on('data', (c) => body += c); req.on('end', () => { sceneDone = body; res.writeHead(204); res.end(); });
    return;
  }
  if (req.method === 'POST' && req.url === '/shot') {
    const chunks = []; req.on('data', (c) => chunks.push(c));
    req.on('end', () => { shotBuf = Buffer.concat(chunks); res.writeHead(204); res.end(); });
    return;
  }
  if (req.method === 'POST' && req.url === '/field') {
    const chunks = []; req.on('data', (c) => chunks.push(c));
    req.on('end', () => {
      dumpPath = path.join(out, 'field_webgpu.bin');
      fs.writeFileSync(dumpPath, Buffer.concat(chunks));
      res.writeHead(204); res.end();
    });
    return;
  }
  const url = new URL(req.url, 'http://x');
  let file = path.join(dist, url.pathname === '/' ? 'index.html' : url.pathname);
  if (fs.existsSync(file) && fs.statSync(file).isDirectory()) file = path.join(file, 'index.html');   // composed docs tree
  if (!file.startsWith(dist) || !fs.existsSync(file)) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream' });
  fs.createReadStream(file).pipe(res);
});

await new Promise((r) => server.listen(0, '127.0.0.1', r));
const port = server.address().port;
const profile = path.join(out, 'chrome-profile');
const flags = [`--user-data-dir=${profile}`, '--no-first-run', '--no-default-browser-check', `--window-size=${windowSize}`];
if (!noUnsafe) flags.unshift('--enable-unsafe-webgpu', '--enable-features=WebGPU', '--disable-gpu-sandbox');
if (!headed) flags.unshift('--headless=new', '--use-angle=default');

async function runPage(url, doneFn, seconds) {
  const child = spawn(chrome, [...flags, url], { stdio: ['ignore', 'pipe', 'pipe'] });
  let err = ''; child.stderr.on('data', (d) => { err += d; });
  const t0 = Date.now();
  while (!doneFn() && Date.now() - t0 < seconds * 1000) await new Promise((r) => setTimeout(r, 250));
  child.kill('SIGTERM');
  return err;
}

if (fullShots !== null) {
  // Full-page screenshots (canvas + DOM) through the DevTools protocol, in the
  // WebGPU-capable headless mode (Chrome's --screenshot pipeline has no WebGPU).
  const port2 = 9333 + Math.floor(Math.random() * 100);
  for (const qs of fullShots.split(',')) {
    const name = (qs || 'main').replace(/[^a-z0-9_]/gi, '_');
    // A leading "/" means a page PATH inside --dist (e.g. a composed docs +
    // marble tree); otherwise it is a query string for the marble page.
    const url = qs.startsWith('/') ? `http://127.0.0.1:${port}${qs}` : `http://127.0.0.1:${port}/?${qs}`;
    const child = spawn(chrome, [...flags, `--remote-debugging-port=${port2}`, url], { stdio: ['ignore', 'pipe', 'pipe'] });
    let png = null;
    try {
      let target = null;
      for (let i = 0; i < 40 && !target; i++) {
        await new Promise((r) => setTimeout(r, 250));
        try {
          const list = await (await fetch(`http://127.0.0.1:${port2}/json/list`)).json();
          target = list.find((t) => t.type === 'page' && t.url.startsWith(`http://127.0.0.1:${port}/`));
        } catch {}
      }
      if (!target) throw new Error('no DevTools page target');
      await new Promise((r) => setTimeout(r, Number(opt('--settle', '5000'))));   // let the scene run (ms)
      const ws = new WebSocket(target.webSocketDebuggerUrl);
      await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
      png = await new Promise((res, rej) => {
        ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id === 1) (d.result ? res(d.result.data) : rej(new Error(JSON.stringify(d.error)))); };
        ws.send(JSON.stringify({ id: 1, method: 'Page.captureScreenshot', params: { format: 'png' } }));
      });
      ws.close();
    } catch (e) { console.log(`fullshot ${name}: FAILED — ${e.message}`); }
    child.kill('SIGTERM');
    if (png) { const f = path.join(out, `page_${name}.png`); fs.writeFileSync(f, Buffer.from(png, 'base64')); console.log(`fullshot ${name}: ${f}`); }
  }
  server.close();
  process.exit(0);
}

if (shotScenes) {
  for (const name of shotScenes.split(',')) {
    shotBuf = null; logLines.length = 0;
    await runPage(`http://127.0.0.1:${port}/?scene=${name}&shot=90&post=1`, () => !!shotBuf, 30);
    if (shotBuf) { const f = path.join(out, `shot_${name}.png`); fs.writeFileSync(f, shotBuf); console.log(`shot ${name}: ${f} (${shotBuf.length} bytes)`); }
    else console.log(`shot ${name}: FAILED — ${logLines.filter((l) => l.startsWith('[error]')).join(' | ') || 'no PNG posted'}`);
  }
  server.close();
  process.exit(0);
}

if (scenesMode) {
  // Every operator scene must run to completion through the query-string API.
  const names = ['drop', 'feed', 'tine', 'vortex', 'rankine', 'wake', 'pinch', 'ripple', 'lamb_oseen', 'scroll'];
  const results = [];
  for (const name of names) {
    let errors = [];
    for (let attempt = 0; attempt < 2; attempt++) {
      sceneDone = null; logLines.length = 0;
      await runPage(`http://127.0.0.1:${port}/?scene=${name}&embed=1&post=1&pace=0`, () => sceneDone === name, 25);   // pace=0: instant, the sweep tests completion not pacing
      errors = logLines.filter((l) => l.startsWith('[error]'));
      // A scene that never completed with NO error is a headless-Chrome launch
      // stall (seen once on a loaded machine: the full sweep otherwise runs in
      // ~7 s) — retry once. A page error is a real failure and is not retried.
      if (sceneDone === name || errors.length) break;
      console.log(`     scene ${name} did not complete — retrying once`);
    }
    const ok = sceneDone === name && errors.length === 0;
    results.push({ name, ok, errors });
    console.log(`${ok ? 'ok  ' : 'FAIL'} scene ${name}${errors.length ? ' — ' + errors.join(' | ') : ''}${sceneDone !== name ? ' (did not complete)' : ''}`);
  }
  server.close();
  fs.writeFileSync(path.join(out, 'scenes.txt'), results.map((r) => `${r.ok ? 'ok' : 'FAIL'} ${r.name}${r.errors.length ? ' ' + r.errors.join(' | ') : ''}`).join('\n') + '\n');
  const failed = results.filter((r) => !r.ok).length;
  console.log(`${results.length - failed}/${results.length} scenes ran clean`);
  process.exit(failed ? 1 : 0);
}

const url = `http://127.0.0.1:${port}/?fielddump=1&post=1`;
console.log(`serving ${dist} at ${url}`);
const chromeErr = await runPage(url, () => !!dumpPath, timeoutS);
server.close();
fs.writeFileSync(path.join(out, 'page_console.txt'), logLines.join('\n') + '\n');
fs.writeFileSync(path.join(out, 'chrome_stderr.txt'), chromeErr);
console.log('--- page console ---'); console.log(logLines.join('\n') || '(nothing reported)');
if (!dumpPath) {
  console.log(`::error::no field dump received within ${timeoutS}s — WebGPU unavailable in this Chrome mode, or the page failed (see page_console.txt)`);
  process.exit(4);
}
console.log(`field dump: ${dumpPath} (${fs.statSync(dumpPath).size} bytes)`);
if (compare && fixture) {
  const r = spawnSync(compare, [dumpPath, fixture, maxTol, meanTol], { encoding: 'utf8' });
  const report = (r.stdout || '') + (r.stderr || '');
  fs.writeFileSync(path.join(out, 'field_gate_webgpu.txt'), `verdict: ${r.status === 0 ? 'PASS' : 'FAIL'}\n\n${report}`);
  console.log(report.trim());
  process.exit(r.status === 0 ? 0 : 1);
}
process.exit(0);
