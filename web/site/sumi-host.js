// sumi-host.js — the sixth host shell (Phase 5 §5): a page of JS around the
// libsumi wasm. Creates the WebGPU device (the only place a browser can), hands
// it to the core with the canvas selector, drives sumi_update/sumi_render from
// requestAnimationFrame, turns pointer/touch/pen events into marble gestures,
// feeds WebMIDI bytes into sumi_push_midi where the browser has it, and serves
// the scene/embed API (?scene=…&param=…&embed=1) the docs embed.
import createSumi from './sumi.js';
import { SCENES, sceneNames } from './scenes.js';
import GUI from './vendor/lil-gui.esm.min.js';   // pinned 0.21.0, MIT (vendor/)

const $ = (id) => document.getElementById(id);
const q = new URLSearchParams(location.search);
const EMBED = q.get('embed') === '1';
const SCENE = q.get('scene');
const FIELDDUMP = q.get('fielddump') === '1';
const POST = q.get('post') === '1';     // tools/web_gate.mjs is listening
const SHOT = Number(q.get('shot') || 0); // POST the canvas as PNG after N frames (evidence)
if (EMBED) document.body.classList.add('embed');
if (POST) {
  // Forward the console to the gate tool so a headless run is debuggable.
  const send = (level, a) => { try { navigator.sendBeacon('/log', `[${level}] ` + a.map(String).join(' ')); } catch {} };
  for (const lv of ['log', 'warn', 'error']) { const orig = console[lv].bind(console); console[lv] = (...a) => { orig(...a); send(lv, a); }; }
  window.addEventListener('error', (e) => send('error', [e.message, e.filename, e.lineno]));
  window.addEventListener('unhandledrejection', (e) => send('error', ['unhandled:', e.reason && (e.reason.stack || e.reason)]));
  console.log('page loaded', location.href, 'webgpu:', !!navigator.gpu);
}

// Gesture tuning — the desktop harness's constants, verbatim.
const DROP_RADIUS = 0.06, TINE_ALPHA = 0.035, VORTEX_RADIUS = 0.18, VORTEX_STRENGTH = 4.0;
const PINCH_DRAG_K = 4.0, DRAG_THRESHOLD_PX = 5, WAKE_TIP_BASE = 0.006, WAKE_TIP_SPAN = 0.030;
const PARAM_ID = { viscosity: 0, expansion: 1, roughness: 2, smoothing_ms: 3, palette: 4, layout: 5,
  sim_scale: 6, bpm: 7, roll_speed: 8, slide_mode: 9, vortex_profile: 10, ripple_bake: 11,
  ripple_angle: 12, pinch_variant: 13, bend_mode: 14, press_mode: 15 };

const status = (t) => { const s = $('status'); if (s) s.textContent = t; };

async function main() {
  const details = `<div class="muted" style="margin-top:6px">origin ${location.origin} · secure context ${window.isSecureContext} · ${navigator.userAgent.replace(/^.*?\) /, '')}</div>`;
  if (!navigator.gpu) {
    $('nogpu').hidden = false;
    if (!window.isSecureContext) {
      // The usual cause: opened by LAN IP over plain http. WebGPU (and
      // WebMIDI) exist only on https:// or localhost.
      $('nogpu-text').innerHTML = `<b>WebGPU needs a secure origin.</b> This page was opened as <code>${location.origin}</code>, ` +
        `which is plain http on a non-localhost address, so the browser hides <code>navigator.gpu</code>. ` +
        `Open it as <code>http://localhost:…</code> on this machine, or serve it over HTTPS for other devices ` +
        `(<code>python3 tools/web_serve.py</code>, then accept the development certificate once).`;
      status('insecure origin — WebGPU hidden');
      console.error('WebGPU hidden: insecure context', location.origin);
    } else {
      $('nogpu-text').innerHTML = `<b>WebGPU is switched off in this browser.</b> The origin is secure, but ` +
        `<code>navigator.gpu</code> is missing. In Chrome/Edge check <code>chrome://gpu</code> (the WebGPU line), ` +
        `<i>Settings → System → Use graphics acceleration when available</i>, <code>chrome://flags/#enable-unsafe-webgpu</code>, ` +
        `and <code>chrome://policy</code> for a managed policy disabling it. Firefox needs 141+, Safari 26.` + details;
      status('WebGPU disabled in this browser');
      console.error('navigator.gpu is undefined on a secure origin', navigator.userAgent);
    }
    return;
  }
  const adapter = await navigator.gpu.requestAdapter();
  if (!adapter) {
    $('nogpu').hidden = false;
    $('nogpu-text').innerHTML = `<b>WebGPU is present but the browser found no GPU adapter.</b> Usually graphics ` +
      `acceleration is off or the GPU is blocklisted: see <code>chrome://gpu</code> and enable ` +
      `<i>Use graphics acceleration when available</i> in <i>Settings → System</i>, then relaunch.` + details;
    status('no WebGPU adapter');
    console.error('requestAdapter() returned null', navigator.userAgent);
    return;
  }
  const device = await adapter.requestDevice();
  // Validation errors never reach console.* on their own — surface them.
  device.addEventListener('uncapturederror', (e) => console.error('WebGPU uncaptured error:', e.error && e.error.message));
  device.lost.then((info) => console.error('WebGPU device lost:', info.reason, info.message));
  const preferred = navigator.gpu.getPreferredCanvasFormat();   // bgra8unorm on most, rgba8unorm on Android
  const fmt = preferred === 'rgba8unorm' ? 1 : 0;

  console.log('adapter:', adapter.info ? `${adapter.info.vendor} ${adapter.info.architecture} ${adapter.info.description}` : '(no info)', 'format:', preferred);
  // The page-created device crosses into the wasm as emdawnwebgpu's
  // preinitialized device: the module imports it at init, and the export glue
  // fetches its handle (sumi_web_create with device = 0). DECISIONS_4 #15.
  const M = await createSumi({ preinitializedWebGPUDevice: device, print: console.log, printErr: console.error });
  console.log('wasm module ready');
  const deviceHandle = 0;

  // The C-ABI through cwrap (the export surface), plus the web shim.
  const C = {
    create: M.cwrap('sumi_web_create', 'number', ['number', 'string', 'number', 'number', 'number', 'number']),
    destroy: M.cwrap('sumi_destroy', null, ['number']),
    resize: M.cwrap('sumi_resize', null, ['number', 'number', 'number', 'number']),
    update: M.cwrap('sumi_update', null, ['number', 'number']),
    render: M.cwrap('sumi_render', null, ['number']),
    midi: M.cwrap('sumi_push_midi', null, ['number', 'number', 'number', 'number']),
    drop: M.cwrap('sumi_add_drop', null, ['number', 'number', 'number', 'number', 'number']),
    tine: M.cwrap('sumi_add_tine', null, ['number', 'number', 'number', 'number', 'number', 'number', 'number']),
    vortex: M.cwrap('sumi_add_vortex', null, ['number', 'number', 'number', 'number', 'number', 'number']),
    wake: M.cwrap('sumi_add_wake', null, ['number', 'number', 'number', 'number', 'number', 'number']),
    pinch: M.cwrap('sumi_add_pinch', null, ['number', 'number', 'number', 'number', 'number']),
    dip: M.cwrap('sumi_trigger_paper_dip', null, ['number']),
    readPrint: M.cwrap('sumi_read_print', 'number', ['number', 'number', 'number', 'number', 'number']),
    mapCC: M.cwrap('sumi_map_cc', null, ['number', 'number', 'number', 'number']),
    probe: M.cwrap('sumi_web_probe', 'number', ['number', 'number', 'number', 'number', 'number']),
    getParam: M.cwrap('sumi_web_get_param', 'number', ['number', 'number']),
    setParam: M.cwrap('sumi_web_set_param', null, ['number', 'number', 'number']),
    fieldScript: M.cwrap('sumi_web_field_script', null, ['number']),
    fieldBegin: M.cwrap('sumi_web_field_begin', 'number', ['number']),
    fieldPoll: M.cwrap('sumi_web_field_poll', 'number', ['number', 'number', 'number', 'number', 'number']),
    version: M.cwrap('sumi_web_version_string', 'string', []),
    dropped: M.cwrap('sumi_dropped_midi_count', 'number', ['number']),
  };

  const canvas = $('sumi');
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  const fit = () => {
    if (FIELDDUMP) { canvas.width = 512; canvas.height = 512; canvas.style.width = '512px'; canvas.style.height = '512px'; return; }
    canvas.width = Math.max(1, Math.round(canvas.clientWidth * dpr));
    canvas.height = Math.max(1, Math.round(canvas.clientHeight * dpr));
  };
  fit();
  const inst = C.create(deviceHandle, '#sumi', fmt, canvas.width, canvas.height, dpr);
  if (!inst) { status('sumi_create failed (see console)'); return; }
  status('');

  // ---- resize ----
  let resizeArmed = false;
  const onResize = () => {
    if (FIELDDUMP) return;
    fit();
    C.resize(inst, canvas.width, canvas.height, dpr);
  };
  window.addEventListener('resize', () => { if (!resizeArmed) { resizeArmed = true; requestAnimationFrame(() => { resizeArmed = false; onResize(); }); } });

  // ---- normalized coordinates (§4.6: one y-down space) ----
  const norm = (e) => {
    const r = canvas.getBoundingClientRect();
    return [ (e.clientX - r.left) / r.width, (e.clientY - r.top) / r.height ];
  };
  const acLen = (ax, ay, bx, by) => {   // canvas-height units
    const r = canvas.getBoundingClientRect();
    const dx = (bx - ax) * r.width / r.height, dy = (by - ay);
    return Math.hypot(dx, dy);
  };
  const aspect = () => { const r = canvas.getBoundingClientRect(); return r.width / Math.max(1, r.height); };

  // ---- gestures ----
  const pointers = new Map();   // pointerId -> {x,y, sx,sy (start), dragged, type, button, pinch}
  let twoFinger = null;         // {ang, dist, cx, cy}
  let wakeTip = 0.02;
  canvas.addEventListener('contextmenu', (e) => e.preventDefault());
  canvas.addEventListener('wheel', (e) => { wakeTip *= e.deltaY < 0 ? 1.15 : 1 / 1.15; wakeTip = Math.min(0.08, Math.max(0.005, wakeTip)); e.preventDefault(); }, { passive: false });

  canvas.addEventListener('pointerdown', (e) => {
    canvas.setPointerCapture(e.pointerId);
    const [x, y] = norm(e);
    pointers.set(e.pointerId, { x, y, sx: x, sy: y, px: e.clientX, py: e.clientY, dragged: false,
      type: e.pointerType, button: e.button, pinch: e.shiftKey });
    const touches = [...pointers.values()].filter(p => p.type === 'touch');
    if (touches.length === 2) {
      const [a, b] = touches;
      twoFinger = { ang: Math.atan2(b.y - a.y, (b.x - a.x) * aspect()), dist: acLen(a.x, a.y, b.x, b.y) };
      a.dragged = b.dragged = true;   // a two-finger gesture never drops on lift
    }
  });
  canvas.addEventListener('pointermove', (e) => {
    const p = pointers.get(e.pointerId);
    if (!p) return;
    const [x, y] = norm(e);
    const moved = Math.hypot(e.clientX - p.px, e.clientY - p.py);
    const touches = [...pointers.values()].filter(t => t.type === 'touch');
    if (p.type === 'touch' && touches.length === 2 && twoFinger) {
      p.x = x; p.y = y; p.px = e.clientX; p.py = e.clientY;
      const [a, b] = touches;
      const ang = Math.atan2(b.y - a.y, (b.x - a.x) * aspect());
      const dist = acLen(a.x, a.y, b.x, b.y);
      let dAng = ang - twoFinger.ang; if (dAng > Math.PI) dAng -= 2 * Math.PI; if (dAng < -Math.PI) dAng += 2 * Math.PI;
      const cx = (a.x + b.x) / 2, cy = (a.y + b.y) / 2;
      // twist → Rankine vortex: R = half the finger separation, ω = the delta
      // (the gesture literally grabs a rigid disk of water, §4.3(3))
      if (Math.abs(dAng) > 0.002) C.vortex(inst, cx, cy, dAng, Math.max(0.05, dist / 2), 1);
      // pinch → Hamiltonian pinch: fold axis = the finger-to-finger line,
      // k from the distance delta (#41)
      const dk = (dist - twoFinger.dist) * 1.5;
      if (Math.abs(dk) > 0.0015) C.pinch(inst, cx, cy, dk, ang);
      twoFinger = { ang, dist };
      return;
    }
    if (moved < DRAG_THRESHOLD_PX) return;
    const mag = acLen(p.x, p.y, x, y);
    if (p.type === 'pen') {
      // the stylus signature: the wake, tip radius from pressure (§4.3(4))
      const a = WAKE_TIP_BASE + WAKE_TIP_SPAN * (e.pressure > 0 ? e.pressure : 0.5);
      C.wake(inst, p.x, p.y, x, y, a);
    } else if (p.button === 2) {
      C.vortex(inst, x, y, mag * VORTEX_STRENGTH, VORTEX_RADIUS, C.getParam(inst, PARAM_ID.vortex_profile));
    } else if (p.button === 1) {
      C.wake(inst, p.x, p.y, x, y, wakeTip);
    } else if (p.pinch) {
      const angle = Math.atan2(y - p.y, (x - p.x) * aspect());
      C.pinch(inst, x, y, mag * PINCH_DRAG_K, angle);
    } else {
      C.tine(inst, p.x, p.y, x, y, TINE_ALPHA, mag);
    }
    p.dragged = true; p.x = x; p.y = y; p.px = e.clientX; p.py = e.clientY;
  });
  const lift = (e) => {
    const p = pointers.get(e.pointerId);
    if (!p) return;
    pointers.delete(e.pointerId);
    if ([...pointers.values()].filter(t => t.type === 'touch').length < 2) twoFinger = null;
    if (!p.dragged && !p.pinch && (p.button === 0 || p.type !== 'mouse')) {
      const [x, y] = norm(e);
      C.drop(inst, x, y, DROP_RADIUS, 0);
    }
  };
  canvas.addEventListener('pointerup', lift);
  canvas.addEventListener('pointercancel', lift);

  // ---- WebMIDI (Chrome/Edge; Safari degrades to gestures-only, silently) ----
  let midiText = '—';
  const midiStatus = (t) => { midiText = t; if (window.__sumiGuiRefresh) window.__sumiGuiRefresh(); };
  if (navigator.requestMIDIAccess) {
    try {
      const access = await navigator.requestMIDIAccess({ sysex: false });
      const wire = () => {
        const names = [];
        for (const input of access.inputs.values()) {
          names.push(input.name);
          input.onmidimessage = (m) => {
            const d = m.data;
            if (!d || d.length < 1 || d.length > 3 || d[0] >= 0xF0) return;   // system messages: skip
            C.midi(inst, d[0], d[1] || 0, d[2] || 0);
          };
        }
        midiStatus(names.length ? names.join(', ') : 'no inputs (plug one in)');
      };
      wire();
      access.onstatechange = wire;
    } catch (err) {
      midiStatus('access denied (' + err.message + ')');
    }
  } else {
    midiStatus('not available in this browser — gestures only');
  }

  // ---- chrome ----
  let printReady = false;
  $('btn-hint-ok')?.addEventListener('click', () => { $('hint').hidden = true; try { localStorage.setItem('sumi-hint', '1'); } catch {} });
  try { if (localStorage.getItem('sumi-hint') === '1') $('hint').hidden = true; } catch {}

  const savePrint = () => {
    const wp = M._malloc(4), hp = M._malloc(4);
    if (!C.readPrint(inst, 0, 0, wp, hp)) { M._free(wp); M._free(hp); return; }
    const w = M.HEAPU32[wp >> 2], h = M.HEAPU32[hp >> 2];
    const bytes = w * h * 4, buf = M._malloc(bytes);
    if (C.readPrint(inst, buf, bytes, wp, hp)) {
      const px = new Uint8ClampedArray(M.HEAPU8.buffer, buf, bytes).slice();
      const cv = document.createElement('canvas'); cv.width = w; cv.height = h;
      cv.getContext('2d').putImageData(new ImageData(px, w, h), 0, 0);
      cv.toBlob((blob) => {
        const a = document.createElement('a'); a.href = URL.createObjectURL(blob);
        a.download = 'midi-sink-print.png'; a.click(); URL.revokeObjectURL(a.href);
      }, 'image/png');
    }
    M._free(buf); M._free(wp); M._free(hp);
  };

  // ---- settings panel (lil-gui) — the desktop settings window, in the browser ----
  // Mirrors desktop/src/settings_ui.cpp's sections that exist in marble mode;
  // persisted per browser in localStorage (the desktop's INI), applied before
  // the first frame. Hidden in embed mode.
  const LAYOUTS = { 'Circle of fifths': 0, 'Chromatic grid': 1, 'Janko': 2, 'Piano roll (horizontal)': 3,
    'Piano roll (vertical)': 4, 'Piano grid': 5 };
  const PALETTES = { 'Sumi black': 0, 'Indigo': 1, 'Ochre': 2 };
  const SETTINGS_KEY = 'sumi-web-settings';
  const st = {
    layout: C.getParam(inst, PARAM_ID.layout), palette: C.getParam(inst, PARAM_ID.palette),
    viscosity: C.getParam(inst, PARAM_ID.viscosity), inkFeed: C.getParam(inst, PARAM_ID.expansion),
    roughness: C.getParam(inst, PARAM_ID.roughness), fullRes: C.getParam(inst, PARAM_ID.sim_scale) >= 0.99,
    bpm: C.getParam(inst, PARAM_ID.bpm), rollSpeed: C.getParam(inst, PARAM_ID.roll_speed),
    bend: C.getParam(inst, PARAM_ID.bend_mode), press: C.getParam(inst, PARAM_ID.press_mode),
    slide: C.getParam(inst, PARAM_ID.slide_mode), pinchVariant: C.getParam(inst, PARAM_ID.pinch_variant),
    vortexProfile: C.getParam(inst, PARAM_ID.vortex_profile),
    rippleAmount: 0, rippleWavelength: 32, rippleAngle: 0,
  };
  try { Object.assign(st, JSON.parse(localStorage.getItem(SETTINGS_KEY) || '{}')); } catch {}
  const applySettings = () => {
    C.setParam(inst, PARAM_ID.layout, st.layout);
    C.setParam(inst, PARAM_ID.palette, st.palette);
    C.setParam(inst, PARAM_ID.viscosity, st.viscosity);
    C.setParam(inst, PARAM_ID.expansion, st.inkFeed);
    C.setParam(inst, PARAM_ID.roughness, st.roughness);
    C.setParam(inst, PARAM_ID.sim_scale, st.fullRes ? 1.0 : 0.75);
    C.setParam(inst, PARAM_ID.bpm, st.bpm);
    C.setParam(inst, PARAM_ID.roll_speed, st.rollSpeed);
    C.setParam(inst, PARAM_ID.bend_mode, st.bend);
    C.setParam(inst, PARAM_ID.ripple_bake, st.bend);          // the Ripple choice bakes (DECISIONS_3 #36)
    C.setParam(inst, PARAM_ID.press_mode, st.press);
    C.setParam(inst, PARAM_ID.slide_mode, st.slide);
    C.setParam(inst, PARAM_ID.pinch_variant, st.pinchVariant);
    C.setParam(inst, PARAM_ID.vortex_profile, st.vortexProfile);
    C.setParam(inst, PARAM_ID.ripple_angle, st.rippleAngle * Math.PI / 180);
    // The ripple dims ship unmapped: route CC 102/103 like the desktop app does.
    C.mapCC(inst, 0xFF, 102, 7); C.mapCC(inst, 0xFF, 103, 8);
    C.midi(inst, 0xB0, 103, st.rippleWavelength);
    C.midi(inst, 0xB0, 102, st.rippleAmount);
    try { localStorage.setItem(SETTINGS_KEY, JSON.stringify(st)); } catch {}
  };
  if (!SCENE) applySettings();   // a scene owns its params; free play restores yours
  const about = { engine: 'libsumi ' + C.version(), midi: '—', frames: '' };
  const actions = {
    dip: () => C.dip(inst),
    savePrint: () => savePrint(),
    reset: () => { try { localStorage.removeItem(SETTINGS_KEY); } catch {} location.reload(); },
  };
  let gui = null;
  if (!EMBED) {
    gui = new GUI({ title: 'midi-sink', width: 300 });
    const f1 = gui.addFolder('Layout & look');
    f1.add(st, 'layout', LAYOUTS).name('Pitch layout').onChange(applySettings);
    f1.add(st, 'palette', PALETTES).name('Palette').onChange(applySettings);
    f1.add(st, 'viscosity', 0, 1, 0.01).name('Viscosity').onChange(applySettings);
    f1.add(st, 'inkFeed', 0.1, 4, 0.01).name('Ink feed (pressure)').onChange(applySettings);
    f1.add(st, 'roughness', 0, 1, 0.01).name('Paper roughness').onChange(applySettings);
    f1.add(st, 'fullRes').name('Full-resolution sim').onChange(applySettings);
    f1.add(st, 'bpm', 20, 300, 1).name('Tempo (rolls)').onChange(applySettings);
    f1.add(st, 'rollSpeed', 0.02, 0.25, 0.005).name('Roll speed').onChange(applySettings);
    const f2 = gui.addFolder('Expression routing');
    f2.add(st, 'bend', { Glide: 0, Ripple: 1 }).name('Per-note bend').onChange(applySettings);
    f2.add(st, 'press', { 'Ink feed': 0, Swirl: 1 }).name('Channel pressure').onChange(applySettings);
    f2.add(st, 'slide', { Hue: 0, Pinch: 1 }).name('Slide (CC 74)').onChange(applySettings);
    f2.add(st, 'pinchVariant', { Saddle: 0, 'Crossed tines': 1 }).name('Pinch style').onChange(applySettings);
    f2.add(st, 'vortexProfile', { Exponential: 0, Rankine: 1 }).name('Vortex profile').onChange(applySettings);
    const f3 = gui.addFolder('Ripple');
    f3.add(st, 'rippleAmount', 0, 127, 1).name('Amount (CC 102)').onChange(applySettings);
    f3.add(st, 'rippleWavelength', 0, 127, 1).name('Wavelength (CC 103)').onChange(applySettings);
    f3.add(st, 'rippleAngle', 0, 180, 1).name('Angle (deg)').onChange(applySettings);
    f3.close();
    const f4 = gui.addFolder('Canvas');
    f4.add(actions, 'dip').name('Paper dip (fresh sheet)');
    const printCtl = f4.add(actions, 'savePrint').name('Save last print as PNG').disable();
    const f5 = gui.addFolder('About');
    f5.add(about, 'engine').name('Engine').disable();
    const midiCtl = f5.add(about, 'midi').name('MIDI inputs').disable();
    const framesCtl = f5.add(about, 'frames').name('Frame').disable();
    f5.add(actions, 'reset').name('Reset settings');
    f5.close();
    if (window.innerWidth < 720) gui.close();
    window.__sumiGuiRefresh = () => { about.midi = midiText; midiCtl.updateDisplay(); };
    window.__sumiGuiFrame = (text, ready) => {
      about.frames = text; framesCtl.updateDisplay();
      if (ready) printCtl.enable(); else printCtl.disable();
    };
  }

  // ---- scene API ----
  const frameWaiters = [];
  const api = {
    drop: (x, y, r, l) => C.drop(inst, x, y, r, l),
    tine: (x0, y0, x1, y1, a, z) => C.tine(inst, x0, y0, x1, y1, a, z),
    vortex: (x, y, s, r, p) => C.vortex(inst, x, y, s, r, p),
    wake: (x0, y0, x1, y1, a) => C.wake(inst, x0, y0, x1, y1, a),
    pinch: (x, y, k, ang) => C.pinch(inst, x, y, k, ang),
    midi: (s, d1, d2) => C.midi(inst, s, d1, d2),
    mapCC: (cc, target) => C.mapCC(inst, 0xFF, cc, target),
    param: (name) => C.getParam(inst, PARAM_ID[name]),
    setParam: (name, v) => C.setParam(inst, PARAM_ID[name], v),
    aspect,
    frames: (n) => new Promise((res) => frameWaiters.push({ n, res })),
    // The layout probe (the tablets' hit-test): the cell under (x, y) on the
    // current layout — scenes use it to put a VOICE where they want a picture.
    probe: (x, y) => {
      const out = M._malloc(16);
      const ok = C.probe(inst, canvas.width / canvas.height, x, y, out);
      const f = new Float32Array(M.HEAPU8.buffer, out, 4);
      const r = ok ? { note: Math.round(f[0]), cx: f[1], cy: f[2], r: f[3] } : null;
      M._free(out);
      return r;
    },
  };
  let scene = null, values = {};
  const sliderHtml = (p, v) =>
    `<label class="slider"><span class="sym">${p.sym}</span>` +
    `<input type="range" data-key="${p.key}" min="${p.min}" max="${p.max}" step="${p.step}" value="${v}" title="${p.label}">` +
    `<span class="val" data-val="${p.key}">${Number(v).toFixed(p.step >= 1 ? 0 : 3)}</span></label>`;
  const runScene = async () => {
    if (!scene) return;
    C.dip(inst);                                 // fresh sheet (deterministic replay)
    await api.frames(2);
    await scene.setup(api, values);
    await api.frames(2);
    console.log('scene done', SCENE, JSON.stringify(values));
    if (POST) { try { navigator.sendBeacon('/scene', SCENE); } catch {} }
  };
  if (SCENE && SCENES[SCENE]) {
    scene = SCENES[SCENE];
    for (const p of scene.params) values[p.key] = q.has(p.key) ? Number(q.get(p.key)) : p.def;
    $('scene-panel').hidden = false;
    $('scene-title').textContent = scene.title;
    $('scene-formula').textContent = scene.formula;
    $('scene-sliders').innerHTML = scene.params.map((p) => sliderHtml(p, values[p.key])).join('');
    $('scene-note').textContent = scene.params.map((p) => `${p.sym} = ${p.label}`).join(' · ');
    $('scene-sliders').addEventListener('input', (e) => {
      const key = e.target.dataset.key; if (!key) return;
      values[key] = Number(e.target.value);
      const p = scene.params.find((s) => s.key === key);
      $('scene-sliders').querySelector(`[data-val="${key}"]`).textContent = values[key].toFixed(p.step >= 1 ? 0 : 3);
      if (scene.live) scene.live(api, values); else runScene();
    });
    $('btn-replay').addEventListener('click', runScene);
    if ($('hint')) $('hint').hidden = true;
  }

  // ---- §4.6 web tier: the canonical field script + non-blocking readback ----
  let dump = null;
  if (FIELDDUMP) {
    C.resize(inst, 512, 512, 1.0);
    dump = { stage: 0, frames: 0 };
  }
  const halfToFloat = (h) => {
    const s = (h & 0x8000) ? -1 : 1, e = (h >> 10) & 0x1F, m = h & 0x3FF;
    if (e === 0) return s * Math.pow(2, -14) * (m / 1024);
    if (e === 31) return m ? NaN : s * Infinity;
    return s * Math.pow(2, e - 15) * (1 + m / 1024);
  };
  const stepDump = () => {
    dump.frames++;
    if (dump.frames % 120 === 0) console.log('fielddump: frame', dump.frames, 'stage', dump.stage);
    if (dump.stage === 0 && dump.frames === 2) {           // settled identity
      C.fieldScript(inst); dump.stage = 1; console.log('fielddump: script queued'); return;
    }
    if (dump.stage === 1 && dump.frames >= 4) {            // the 7 passes drained
      const ok = C.fieldBegin(inst);
      console.log('fielddump: readback begin ->', ok);
      if (ok) dump.stage = 2;
      else if (dump.frames > 30) { console.error('fielddump: readback never started'); dump.stage = 4; }
      return;
    }
    if (dump.stage === 2) {
      const wp = M._malloc(4), hp = M._malloc(4);
      C.fieldPoll(inst, 0, 0, wp, hp);
      const w = M.HEAPU32[wp >> 2], h = M.HEAPU32[hp >> 2];
      const bytes = w * h * 8, buf = M._malloc(bytes);
      const st = C.fieldPoll(inst, buf, bytes, wp, hp);
      if (dump.frames % 30 === 0 || st !== 1) console.log('fielddump: poll ->', st, w, h);
      if (st === 0) { console.error('fielddump: readback failed'); dump.stage = 4; }
      if (st === 2) {
        const halves = new Uint16Array(M.HEAPU8.buffer, buf, w * h * 4).slice();
        const out = new ArrayBuffer(8 + w * h * 16);
        new Uint32Array(out, 0, 2).set([w, h]);
        const f = new Float32Array(out, 8);
        for (let i = 0; i < halves.length; i++) f[i] = halfToFloat(halves[i]);
        const blob = new Blob([out], { type: 'application/octet-stream' });
        if (POST) {
          fetch('/field', { method: 'POST', body: blob }).then(() => console.log('field dump posted', w, h));
        } else {
          const a = document.createElement('a'); a.href = URL.createObjectURL(blob);
          a.download = 'field_webgpu.bin'; a.click();
        }
        window.sumiFieldDump = { w, h, bytes: out.byteLength };
        document.title = 'fielddump-done';
        status(`field dump ${w}x${h} downloaded (field_webgpu.bin)`);
        dump.stage = 3;
      }
      M._free(buf); M._free(wp); M._free(hp);
    }
  };

  // ---- frame loop ----
  let last = performance.now(), frames = 0, firstMarked = false, fpsAcc = 0, fpsN = 0;
  const loop = (now) => {
    const dt = Math.min(0.1, Math.max(0, (now - last) / 1000)); last = now;
    C.update(inst, dt);
    C.render(inst);
    frames++;
    if (!firstMarked) { firstMarked = true; performance.mark('first-marble'); }
    for (let i = frameWaiters.length - 1; i >= 0; i--) { if (--frameWaiters[i].n <= 0) { frameWaiters.splice(i, 1)[0].res(); } }
    if (dump && dump.stage < 3) stepDump();
    if (SHOT && frames === SHOT) {
      // Same task as the draw: the WebGPU canvas texture is still readable.
      canvas.toBlob((b) => { if (b && POST) fetch('/shot', { method: 'POST', body: b }).then(() => console.log('shot posted')); }, 'image/png');
    }
    fpsAcc += dt; fpsN++;
    if (fpsAcc >= 1) {
      const text = `${(fpsN / fpsAcc).toFixed(0)} fps · ${canvas.width}×${canvas.height} · dropped MIDI ${C.dropped(inst)}`;
      fpsAcc = 0; fpsN = 0;
      const wp = M._malloc(4), hp = M._malloc(4);
      printReady = !!C.readPrint(inst, 0, 0, wp, hp);
      M._free(wp); M._free(hp);
      if (window.__sumiGuiFrame) window.__sumiGuiFrame(text, printReady);
    }
    requestAnimationFrame(loop);
  };
  requestAnimationFrame(loop);
  if (scene) runScene();
}

main().catch((e) => { console.error(e); status('error: ' + e.message); });
