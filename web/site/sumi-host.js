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
// Phase 9 step 66: ?layout=N (a transient override, never stored), ?overlay=1|0 (the layout overlay),
// ?replay=<url> (a .sumireplay played on the scripted clock), ?replaydump=1 (the field after it, as the gate reads
// it, at the recording's size), ?replaywall=<hz> (the negative test: re-bucketed by wall time), ?pace=0 (every frame at once).
const LAYOUT_Q = q.has('layout') ? Number(q.get('layout')) : null;
const OVERLAY_Q = q.has('overlay') ? q.get('overlay') === '1' : null;
const REPLAY = q.get('replay');
const REPLAYDUMP = q.get('replaydump') === '1';
const REPLAYWALL = Number(q.get('replaywall') || 0);
const PACE0 = q.get('pace') === '0';
const VALVES_Q = q.get('valves');   // the evidence: "1,3" presses valves 1 and 3 (CC 110/112) at start; ?slide=0.5 sets the slide (CC 113)
const SLIDE_Q = q.has('slide') ? Number(q.get('slide')) : null;
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
// v0.6 pressure gesture (DECISIONS_4 #49) — same constants as desktop and the tablets.
const PRESS_TRAVEL = 0.15;   // the feed / swirl rates live in the core since #75 (sumi_gesture_press)
const LONG_PRESS_MS = 250;
const PARAM_ID = { viscosity: 0, expansion: 1, roughness: 2, smoothing_ms: 3, palette: 4, layout: 5,
  sim_scale: 6, bpm: 7, roll_speed: 8, slide_mode: 9, vortex_profile: 10, ripple_bake: 11,
  ripple_angle: 12, pinch_variant: 13, bend_mode: 14, press_mode: 15, wake_profile: 16, wake_spread: 17,
  torsion_sweep: 18, chladni_cell: 19, burst_age: 20, burst_life: 21, burst_order: 22,
  spark_stack: 23, spark_profile: 24, spark_shear: 25, spark_tau: 26,
  chirikov_kmax: 27, chirikov_periods: 28, chirikov_eps: 29, medium: 30, anod_glow: 31, anod_pitch: 32, chladni_mode: 33,
  paper_tint_r: 34, paper_tint_g: 35, paper_tint_b: 36, fiber_scale: 37, anod_dark: 38, anod_grain: 39, anod_bloom: 40, anod_bloom_levels: 41, anod_drop: 42,
  trumpet_arc: 43, string_tuning: 44 };   // Phase 9 (the web at step 66): the brass arrangement, the strings' tuning

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
    chladni: M.cwrap('sumi_add_chladni', null, ['number', 'number', 'number', 'number', 'number', 'number', 'number']),
    burst: M.cwrap('sumi_add_burst', null, ['number', 'number', 'number', 'number', 'number', 'number', 'number']),
    spark: M.cwrap('sumi_add_spark', null, ['number', 'number', 'number', 'number', 'number', 'number', 'number']),
    sparkShear: M.cwrap('sumi_add_spark_shear', null, ['number', 'number', 'number', 'number', 'number', 'number', 'number', 'number', 'number']),
    chirikov: M.cwrap('sumi_add_chirikov', null, ['number', 'number', 'number', 'number', 'number', 'number', 'number']),
    // #75: the medium-aware marble gestures (Sumi: the operator calls as before; Anod: the gesture table)
    gTap: M.cwrap('sumi_gesture_tap', null, ['number', 'number', 'number', 'number']),
    gPinch: M.cwrap('sumi_gesture_pinch', null, ['number', 'number', 'number', 'number', 'number', 'number']),
    gTwist: M.cwrap('sumi_gesture_twist', null, ['number', 'number', 'number', 'number', 'number', 'number']),
    gPress: M.cwrap('sumi_gesture_press', 'number', ['number', 'number', 'number', 'number', 'number', 'number', 'number']),
    gPressEnd: M.cwrap('sumi_gesture_press_end', null, ['number']),
    dip: M.cwrap('sumi_trigger_paper_dip', null, ['number']),
    readPrint: M.cwrap('sumi_read_print', 'number', ['number', 'number', 'number', 'number', 'number']),
    mapCC: M.cwrap('sumi_map_cc', null, ['number', 'number', 'number', 'number']),
    setInputMode: M.cwrap('sumi_set_input_mode', null, ['number', 'number']),
    probe: M.cwrap('sumi_web_probe', 'number', ['number', 'number', 'number', 'number', 'number']),
    layoutState: M.cwrap('sumi_web_layout_state', null, ['number', 'number']),   // step 66: the engine's fingering for the overlay
    // step 66: replay playback — the shim's one open recording (replay/, the shells' library, in the wasm)
    rOpen: M.cwrap('sumi_web_replay_open', 'number', ['number', 'number']),
    rClose: M.cwrap('sumi_web_replay_close', null, []),
    rBanner: M.cwrap('sumi_web_replay_banner', 'string', []),
    rStat: M.cwrap('sumi_web_replay_stat', 'number', ['number']),
    rBegin: M.cwrap('sumi_web_replay_begin', 'number', ['number', 'number']),
    rStep: M.cwrap('sumi_web_replay_step', 'number', ['number', 'number']),
    rRebucket: M.cwrap('sumi_web_replay_rebucket', 'number', ['number']),
    getParam: M.cwrap('sumi_web_get_param', 'number', ['number', 'number']),
    setParam: M.cwrap('sumi_web_set_param', null, ['number', 'number', 'number']),
    fieldScript: M.cwrap('sumi_web_field_script', null, ['number']),
    fieldBegin: M.cwrap('sumi_web_field_begin', 'number', ['number']),
    fieldPoll: M.cwrap('sumi_web_field_poll', 'number', ['number', 'number', 'number', 'number', 'number']),
    version: M.cwrap('sumi_web_version_string', 'string', []),
    dropped: M.cwrap('sumi_dropped_midi_count', 'number', ['number']),
  };

  const canvas = $('sumi');
  const overlayCanvas = $('overlay');   // step 66: the layout overlay, the same pixels as the water
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  let replayLab = null;   // step 66: the gate's replay holds the canvas at the recording's size
  let replay = null;      // step 66: { acc, flags, done } while a recording plays
  const fit = () => {
    if (replayLab) return;
    if (FIELDDUMP) { canvas.width = 512; canvas.height = 512; canvas.style.width = '512px'; canvas.style.height = '512px'; }
    else {
      canvas.width = Math.max(1, Math.round(canvas.clientWidth * dpr));
      canvas.height = Math.max(1, Math.round(canvas.clientHeight * dpr));
    }
    overlayCanvas.width = canvas.width; overlayCanvas.height = canvas.height;
    overlayCanvas.style.width = canvas.style.width; overlayCanvas.style.height = canvas.style.height;
  };
  fit();
  const inst = C.create(deviceHandle, '#sumi', fmt, canvas.width, canvas.height, dpr);
  if (!inst) { status('sumi_create failed (see console)'); return; }
  status('');

  // ---- resize ----
  let resizeArmed = false;
  const onResize = () => {
    if (FIELDDUMP || replayLab) return;
    fit();
    C.resize(inst, canvas.width, canvas.height, dpr);
    drawOverlay(true);
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

  // ---- Phase 9 step 66 (INSTRUMENT §4; DECISIONS_8 #25): THE LAYOUT OVERLAY ----
  // The keyed layouts' cells drawn over the water from the probe — a sweep of the sheet, one circle and note
  // name per cell (the tablets' lattice, the desktop's plate guide); the trumpet's and trombone's partials
  // under the fingering the engine holds (the probe shim carries the state), the theremin's pitch axis with a
  // tick at every C. Visual only: Play stays web-deferred (INSTRUMENT §4). Redrawn when the layout, the size,
  // the brass params, the medium or the fingering change — the fingering is polled every few frames.
  const ov = { show: (() => { if (OVERLAY_Q !== null) return OVERLAY_Q; try { return localStorage.getItem('sumi-web-overlay') !== '0'; } catch { return true; } })() };
  const probeBuf = M._malloc(20), stateBuf = M._malloc(8);
  const NOTE_NAMES = ['C', 'C♯', 'D', 'E♭', 'E', 'F', 'F♯', 'G', 'A♭', 'A', 'B♭', 'B'];
  const noteName = (n) => NOTE_NAMES[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 1);
  const KEYED = new Set([1, 2, 5, 8, 9, 10, 11, 12]);
  let ovKey = '';
  const fingering = () => { C.layoutState(inst, stateBuf); const f = new Float32Array(M.HEAPU8.buffer, stateBuf, 2); return { valves: Math.round(f[0]), slide: f[1] }; };
  const drawOverlay = (force) => {
    const ctx = overlayCanvas.getContext('2d');
    const layout = Math.round(C.getParam(inst, PARAM_ID.layout));
    const W = overlayCanvas.width, H = overlayCanvas.height;
    if (!ov.show || !KEYED.has(layout) || W < 2 || H < 2) { if (ovKey) { ctx.clearRect(0, 0, W, H); ovKey = ''; } return; }
    const fing = fingering();
    const dark = C.getParam(inst, PARAM_ID.medium) === 1;
    const key = [layout, W, H, fing.valves, fing.slide.toFixed(3), C.getParam(inst, PARAM_ID.trumpet_arc), C.getParam(inst, PARAM_ID.string_tuning), dark].join('|');
    if (!force && key === ovKey) return;
    ovKey = key;
    ctx.clearRect(0, 0, W, H);
    const a = W / H;
    const f = new Float32Array(M.HEAPU8.buffer, probeBuf, 5);
    ctx.lineWidth = Math.max(1, H / 600);
    ctx.strokeStyle = dark ? 'rgba(255,255,255,0.45)' : 'rgba(20,20,30,0.42)';
    ctx.fillStyle = dark ? 'rgba(255,255,255,0.8)' : 'rgba(20,20,30,0.72)';
    if (layout === 12) {   // the theremin: no cells — the pitch axis across the width, a tick at every C
      const y = 0.5;
      ctx.beginPath(); ctx.moveTo(0, y * H); ctx.lineTo(W, y * H); ctx.stroke();
      ctx.font = `${Math.max(10, H / 50)}px system-ui, sans-serif`; ctx.textAlign = 'center'; ctx.textBaseline = 'alphabetic';
      let last = -1;
      for (let i = 0; i <= 600; i++) {
        const x = i / 600;
        if (!C.probe(inst, a, x, y, probeBuf)) continue;
        const note = Math.round(f[0]);
        if (note === last) continue;
        last = note;
        if (note % 12 === 0) {
          ctx.beginPath(); ctx.moveTo(x * W, (y - 0.03) * H); ctx.lineTo(x * W, (y + 0.03) * H); ctx.stroke();
          ctx.fillText(noteName(note), x * W, (y + 0.065) * H);
        }
      }
    } else {
      const cells = new Map();
      const nx = 160, ny = Math.max(40, Math.round(160 / a));
      for (let j = 0; j < ny; j++) for (let i = 0; i < nx; i++) {
        if (!C.probe(inst, a, (i + 0.5) / nx, (j + 0.5) / ny, probeBuf)) continue;
        const k = f[1].toFixed(4) + ',' + f[2].toFixed(4);
        if (!cells.has(k)) cells.set(k, { note: Math.round(f[0]), cx: f[1], cy: f[2], r: f[3] });
      }
      ctx.font = `${Math.max(9, H / 60)}px system-ui, sans-serif`; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      for (const c of cells.values()) {
        const px = c.cx * W, py = c.cy * H, pr = c.r * H;
        ctx.beginPath(); ctx.arc(px, py, pr, 0, Math.PI * 2); ctx.stroke();
        if (pr >= 9) ctx.fillText(noteName(c.note), px, py);
      }
    }
    if (layout === 8 || layout === 9) {   // the fingering the engine holds (the CCs' state), as the tablets' panel mirrors it
      ctx.font = `${Math.max(11, H / 40)}px system-ui, sans-serif`; ctx.textAlign = 'left'; ctx.textBaseline = 'alphabetic';
      const v = fing.valves, valves = [1, 2, 3].filter((k) => v & (1 << (k - 1)));
      ctx.fillText(layout === 8 ? `valves ${valves.length ? valves.join('·') : 'open'}` : `slide ${fing.slide.toFixed(2)}`, 12, H - 14);
    }
  };

  // ---- gestures ----
  const pointers = new Map();   // pointerId -> {x,y, sx,sy (start), dragged, type, button, pinch}
  let twoFinger = null;         // {ang, dist, cx, cy}
  let wakeTip = 0.02;
  canvas.addEventListener('contextmenu', (e) => e.preventDefault());
  canvas.addEventListener('wheel', (e) => { wakeTip *= e.deltaY < 0 ? 1.15 : 1 / 1.15; wakeTip = Math.min(0.08, Math.max(0.005, wakeTip)); e.preventDefault(); }, { passive: false });

  // The pressure gesture: the press lays a drop and becomes Play mode's bipolar
  // Y axis — hold or push up = feed, pull down = swirl (v0.6, #49). Mouse:
  // Shift + right button. Touch: a long press (250 ms without travel).
  const beginPressure = (p, x, y, clientY) => {
    C.gTap(inst, x, y, DROP_RADIUS);   // #75: the press starts as a tap (Anod: the strike)
    p.pressure = { x, y, R: DROP_RADIUS, cy0: clientY, cy: clientY };
    p.dragged = true;   // never a second drop on lift
  };
  const pressureTick = (dt) => {
    const h = Math.max(1, canvas.getBoundingClientRect().height);
    for (const p of pointers.values()) {
      const pr = p.pressure; if (!pr) continue;
      const dy = (pr.cy0 - pr.cy) / h;                     // up = positive, canvas heights
      const up = Math.min(1, Math.max(0, dy / PRESS_TRAVEL)), down = Math.min(1, Math.max(0, -dy / PRESS_TRAVEL));
      // #75: the core plays the frame by the medium (Sumi: feed / Lamb–Oseen swirl; Anod: torsion feed / stir)
      pr.R = C.gPress(inst, pr.x, pr.y, pr.R, up, down, dt);
    }
  };
  canvas.addEventListener('pointerdown', (e) => {
    if (replay && !replay.done) return;   // step 66: a replay plays — the viewer watches
    canvas.setPointerCapture(e.pointerId);
    const [x, y] = norm(e);
    const p = { x, y, sx: x, sy: y, px: e.clientX, py: e.clientY, dragged: false,
      type: e.pointerType, button: e.button, pinch: e.shiftKey, pressure: null, timer: 0 };
    pointers.set(e.pointerId, p);
    const touches = [...pointers.values()].filter(t => t.type === 'touch');
    if (touches.length === 2) {
      const [a, b] = touches;
      twoFinger = { ang: Math.atan2(b.y - a.y, (b.x - a.x) * aspect()), dist: acLen(a.x, a.y, b.x, b.y) };
      a.dragged = b.dragged = true;   // a two-finger gesture never drops on lift
      clearTimeout(a.timer); clearTimeout(b.timer);
    }
    if (e.pointerType === 'mouse' && e.button === 2 && e.shiftKey) beginPressure(p, x, y, e.clientY);
    else if (e.pointerType === 'touch' && touches.length === 1) {
      p.timer = setTimeout(() => {
        if (pointers.get(e.pointerId) === p && !p.dragged && !twoFinger) beginPressure(p, p.x, p.y, p.py);
      }, LONG_PRESS_MS);
    }
  });
  canvas.addEventListener('pointermove', (e) => {
    const p = pointers.get(e.pointerId);
    if (!p) return;
    if (p.pressure) { p.pressure.cy = e.clientY; return; }   // the press modulates, it does not draw
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
      if (Math.abs(dAng) > 0.002) C.gTwist(inst, cx, cy, dAng, Math.max(0.05, dist / 2), 1);   // #75: Anod the torsion vortex
      // pinch → Hamiltonian pinch: fold axis = the finger-to-finger line,
      // k from the distance delta (#41)
      const dk = (dist - twoFinger.dist) * 1.5;
      if (Math.abs(dk) > 0.0015) C.gPinch(inst, cx, cy, dk, ang, dist);   // #75: Anod the burst
      twoFinger = { ang, dist };
      return;
    }
    if (moved < DRAG_THRESHOLD_PX) return;
    clearTimeout(p.timer);   // travel before the long press fires = a stroke, not a press
    const mag = acLen(p.x, p.y, x, y);
    if (p.type === 'pen') {
      // the stylus signature: the wake, tip radius from pressure (§4.3(4))
      const a = WAKE_TIP_BASE + WAKE_TIP_SPAN * (e.pressure > 0 ? e.pressure : 0.5);
      C.wake(inst, p.x, p.y, x, y, a);
    } else if (p.button === 2) {
      C.gTwist(inst, x, y, mag * VORTEX_STRENGTH, VORTEX_RADIUS, C.getParam(inst, PARAM_ID.vortex_profile));   // #75
    } else if (p.button === 1) {
      C.wake(inst, p.x, p.y, x, y, wakeTip);
    } else if (p.pinch) {
      const angle = Math.atan2(y - p.y, (x - p.x) * aspect());
      C.gPinch(inst, x, y, mag * PINCH_DRAG_K, angle, 2 * VORTEX_RADIUS);   // #75: a mouse has no finger span
    } else {
      C.tine(inst, p.x, p.y, x, y, TINE_ALPHA, mag);
    }
    p.dragged = true; p.x = x; p.y = y; p.px = e.clientX; p.py = e.clientY;
  });
  const lift = (e) => {
    const p = pointers.get(e.pointerId);
    if (!p) return;
    clearTimeout(p.timer);
    pointers.delete(e.pointerId);
    if ([...pointers.values()].filter(t => t.type === 'touch').length < 2) twoFinger = null;
    if (p.pressure) C.gPressEnd(inst);   // #75: the press lets go of a stir it set
    if (!p.dragged && !p.pinch && (p.button === 0 || p.type !== 'mouse')) {
      const [x, y] = norm(e);
      C.gTap(inst, x, y, DROP_RADIUS);   // #75: the medium's tap (Sumi the drop, Anod the strike)
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
            if (replay && !replay.done) return;   // step 66: the replay owns the loopback
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
  // Mirrors desktop/src/settings_ui.cpp's sections that exist in marble mode.
  // Step 44b (QOL §1/§3): the session — the core's params and palette, the
  // CC-map mirror, the routed controls' values, the input dialect — is
  // persisted per browser as PRESET JSON written by the one C serializer
  // (compiled into the wasm: the desktop's and the tablets' code path), under
  // localStorage 'sumi-web-session'; named presets under 'sumi-web-presets';
  // export downloads the file, import reads one (the desktop's and the iPad's
  // load here as they are). Applied before the first frame; hidden in embed mode.
  const LAYOUTS = { 'Circle of fifths': 0, 'Chromatic grid': 1, 'Janko': 2, 'Piano roll (left)': 3,
    'Piano roll (top)': 4, 'Piano grid': 5, 'Piano roll (right)': 6, 'Piano roll (bottom)': 7,
    // Phase 9 (steps 60–61; the web at 66): the instruments — overlays here, Play stays on the tablets and the desktop's MIDI
    'Trumpet': 8, 'Trombone': 9, 'Wicki–Hayden': 10, 'Strings': 11, 'Theremin': 12 };
  const SESSION_KEY = 'sumi-web-session', PRESETS_KEY = 'sumi-web-presets', LEGACY_KEY = 'sumi-web-settings';
  const lsGet = (k) => { try { return localStorage.getItem(k); } catch { return null; } };
  const lsSet = (k, v) => { try { localStorage.setItem(k, v); } catch {} };
  const persist = !SCENE && !FIELDDUMP && q.get('presetcheck') !== '1' && LAYOUT_Q === null && !REPLAY;   // a scene / the gates / a transient layout / a replay own their params
  const P = {   // the preset/palette shim (web/sumi_web.cpp)
    capture: M.cwrap('sumi_web_preset_capture', null, ['number']),
    setInput: M.cwrap('sumi_web_preset_set_input', null, ['number']),
    clearHost: M.cwrap('sumi_web_preset_clear_host', null, []),
    addCC: M.cwrap('sumi_web_preset_add_cc', null, ['number', 'number', 'number']),
    addControl: M.cwrap('sumi_web_preset_add_control', null, ['number', 'number']),
    write: M.cwrap('sumi_web_preset_write', 'string', ['string']),
    read: M.cwrap('sumi_web_preset_read', 'number', ['string']),
    apply: M.cwrap('sumi_web_preset_apply', null, ['number']),
    input: M.cwrap('sumi_web_preset_input', 'number', []),
    ccCount: M.cwrap('sumi_web_preset_cc_count', 'number', []),
    ccAt: M.cwrap('sumi_web_preset_cc_at', 'number', ['number']),
    ctlCount: M.cwrap('sumi_web_preset_control_count', 'number', []),
    ctlAt: M.cwrap('sumi_web_preset_control_at', 'number', ['number']),
    palGet: M.cwrap('sumi_web_palette_get', null, ['number', 'number']),
    palSet: M.cwrap('sumi_web_palette_set', null, ['number', 'number']),
    palPreset: M.cwrap('sumi_web_palette_preset', 'string', ['number', 'number', 'number']),
    clearMap: M.cwrap('sumi_clear_cc_map', null, ['number']),
  };
  const palBuf = M._malloc(45 * 4);
  const palRead = () => { P.palGet(inst, palBuf); return Array.from(new Float32Array(M.HEAPU8.buffer, palBuf, 45)); };
  const palWrite = (a) => { new Float32Array(M.HEAPU8.buffer, palBuf, 45).set(a); P.palSet(inst, palBuf); };
  const libPalette = (medium, i) => { const name = P.palPreset(medium, i, palBuf); return name ? { name, floats: Array.from(new Float32Array(M.HEAPU8.buffer, palBuf, 45)) } : null; };
  const libNames = (medium) => { const n = []; for (let i = 0; i < 16; i++) { const e = libPalette(medium, i); if (!e) break; n.push(e.name); } return n; };
  const lin2srgb = (v) => { v = Math.min(1, Math.max(0, v)); return v <= 0.0031308 ? 12.92 * v : 1.055 * Math.pow(v, 1 / 2.4) - 0.055; };
  const srgb2lin = (v) => { v = Math.min(1, Math.max(0, v)); return v <= 0.04045 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4); };

  // The host state: the desktop's default CC map (app_settings_default_routes) and control values.
  const DEFAULT_ROUTES = [[255, 1, 0], [255, 2, 6], [255, 7, 6], [255, 11, 6], [255, 26, 0], [255, 24, 1], [255, 22, 2],
    [255, 27, 9], [255, 25, 10], [255, 23, 11], [255, 20, 12], [255, 21, 13], [255, 28, 8], [255, 29, 7],
    [255, 102, 7], [255, 103, 8], [255, 104, 14], [255, 105, 15], [255, 106, 16], [255, 107, 17], [255, 108, 18], [255, 109, 19]];
  const DEFAULT_CONTROLS = [[7, 0], [8, 32], [16, 0], [17, 0], [18, 64], [19, 0]];   // the desktop's order: ripple amt/λ, Chladni A/B, spark k, Chirikov
  const host = { routes: DEFAULT_ROUTES.map((r) => r.slice()), controls: DEFAULT_CONTROLS.map((c) => c.slice()), inputMode: 1 };
  const sent = new Map();   // ctl -> last value pushed
  const applyRoutes = () => { P.clearMap(inst); for (const [ch, cc, t] of host.routes) C.mapCC(inst, ch, cc, t); sent.clear(); };
  const control = (ctl) => (host.controls.find((c) => c[0] === ctl) || [ctl, 0])[1];
  const setControl = (ctl, v) => { const c = host.controls.find((c) => c[0] === ctl); if (c) c[1] = v; else host.controls.push([ctl, v]); };
  const sendControls = () => {
    for (const [ctl, v] of host.controls) {
      if (sent.get(ctl) === v) continue;
      const r = host.routes.find((r) => r[2] === ctl);
      if (r) { C.midi(inst, 0xB0, r[1], v); sent.set(ctl, v); }
    }
  };
  const sessionJSON = (name) => {
    P.capture(inst); P.setInput(host.inputMode); P.clearHost();
    for (const [ch, cc, t] of host.routes) P.addCC(ch, cc, t);
    for (const [ctl, v] of host.controls) P.addControl(ctl, v);
    return P.write(name);
  };
  const saveSession = () => { if (persist) lsSet(SESSION_KEY, sessionJSON('last session')); };
  // Preset text over the session as it stands (the schema rule). true = applied.
  const importJSON = (text) => {
    P.capture(inst); P.setInput(host.inputMode); P.clearHost();
    for (const [ch, cc, t] of host.routes) P.addCC(ch, cc, t);
    for (const [ctl, v] of host.controls) P.addControl(ctl, v);
    if (!P.read(text)) return false;
    P.apply(inst);   // params, input, palette, CC map
    host.inputMode = P.input();
    host.routes = []; for (let i = 0; i < P.ccCount(); i++) { const x = P.ccAt(i); host.routes.push([x & 0xFF, (x >> 8) & 0xFF, x >>> 16]); }
    host.controls = []; for (let i = 0; i < P.ctlCount(); i++) { const x = P.ctlAt(i); host.controls.push([x & 0xFFFF, x >>> 16]); }
    sent.clear(); sendControls();
    return true;
  };

  // The GUI's model: read from the core (after any restore), written back through setParam.
  const PARAMS = [['layout', 'layout'], ['palette', 'palette'], ['viscosity', 'viscosity'], ['inkFeed', 'expansion'],
    ['bpm', 'bpm'], ['rollSpeed', 'roll_speed'], ['bend', 'bend_mode'], ['press', 'press_mode'], ['slide', 'slide_mode'],
    ['pinchVariant', 'pinch_variant'], ['vortexProfile', 'vortex_profile'], ['torsionSweep', 'torsion_sweep'],
    ['wakeProfile', 'wake_profile'], ['wakeSpread', 'wake_spread'], ['medium', 'medium'], ['anodGlow', 'anod_glow'],
    ['anodDrop', 'anod_drop'], ['roughness', 'roughness'], ['fiberScale', 'fiber_scale'], ['anodDark', 'anod_dark'],
    ['anodGrain', 'anod_grain'], ['anodBloom', 'anod_bloom'], ['anodBloomLevels', 'anod_bloom_levels'],
    ['trumpetArc', 'trumpet_arc'], ['stringTuning', 'string_tuning']];   // step 66
  let layoutRows = () => {};   // step 66: the GUI rows that follow the layout (the brass arrangement, the tuning, the fingering)
  const st = {};
  const readModel = () => {
    // six significant digits: the float32 behind 0.479999989 displays as 0.48 (and writes back as the same float32)
    for (const [k, id] of PARAMS) st[k] = Number(C.getParam(inst, PARAM_ID[id]).toPrecision(6));
    st.fullRes = C.getParam(inst, PARAM_ID.sim_scale) >= 0.99;
    st.rippleAngle = Math.round(C.getParam(inst, PARAM_ID.ripple_angle) * 180 / Math.PI);
    const pitch = C.getParam(inst, PARAM_ID.anod_pitch); st.gridLines = pitch > 0 ? Math.round(1 / pitch) : 0;
    st.tint = { r: lin2srgb(C.getParam(inst, PARAM_ID.paper_tint_r)), g: lin2srgb(C.getParam(inst, PARAM_ID.paper_tint_g)), b: lin2srgb(C.getParam(inst, PARAM_ID.paper_tint_b)) };
    st.inputMode = host.inputMode;
    st.rippleAmount = control(7); st.rippleWavelength = control(8);
  };
  const applySettings = () => {
    if (replay && !replay.done) return;   // step 66: the physics is the recording's while it plays
    for (const [k, id] of PARAMS) C.setParam(inst, PARAM_ID[id], st[k]);
    if (st.bend === 1) C.setParam(inst, PARAM_ID.ripple_bake, 1); else if (st.bend === 0) C.setParam(inst, PARAM_ID.ripple_bake, 0);   // the Ripple choice bakes (DECISIONS_3 #36)
    C.setParam(inst, PARAM_ID.sim_scale, st.fullRes ? 1.0 : 0.75);
    C.setParam(inst, PARAM_ID.ripple_angle, st.rippleAngle * Math.PI / 180);
    C.setParam(inst, PARAM_ID.anod_pitch, st.gridLines < 8 ? 0 : 1 / Math.min(256, st.gridLines));
    C.setParam(inst, PARAM_ID.paper_tint_r, srgb2lin(st.tint.r)); C.setParam(inst, PARAM_ID.paper_tint_g, srgb2lin(st.tint.g)); C.setParam(inst, PARAM_ID.paper_tint_b, srgb2lin(st.tint.b));
    if (host.inputMode !== st.inputMode) { host.inputMode = st.inputMode; C.setInputMode(inst, st.inputMode); }
    setControl(7, st.rippleAmount); setControl(8, st.rippleWavelength);
    sendControls();
    saveSession();
    layoutRows(); drawOverlay(false);   // step 66
  };

  // Restore: the last session; else the 1.x-and-before settings object, once.
  applyRoutes(); C.setInputMode(inst, host.inputMode);
  if (persist) {
    const saved = lsGet(SESSION_KEY);
    if (saved && importJSON(saved)) { /* restored */ }
    else {
      readModel();
      try {
        const old = JSON.parse(lsGet(LEGACY_KEY) || 'null');
        if (old) { for (const k of Object.keys(old)) if (k in st) st[k] = old[k]; applySettings(); }
      } catch {}
    }
    sendControls();
    saveSession();
  }
  if (LAYOUT_Q !== null && LAYOUT_Q >= 0 && LAYOUT_Q <= 12) C.setParam(inst, PARAM_ID.layout, LAYOUT_Q);   // step 66: ?layout=N, transient
  if (VALVES_Q) for (const v of VALVES_Q.split(',')) { const k = Number(v); if (k >= 1 && k <= 3) C.midi(inst, 0xB0, 109 + k, 127); }   // the fingering CCs, as a controller sends them
  if (SLIDE_Q !== null) C.midi(inst, 0xB0, 113, Math.round(Math.min(1, Math.max(0, SLIDE_Q)) * 127));
  readModel();
  if (q.get('presetcheck') === '1') {
    // tools/web_gate.mjs --preset <file>: the file through this page's import path and back out.
    fetch('/preset-in').then((r) => r.text()).then((text) => {
      const ok = importJSON(text);
      if (ok) { readModel(); refreshAll(); }   // the panel shows what was imported
      let name = ''; try { name = JSON.parse(text).name || ''; } catch {}
      const out = ok ? sessionJSON(name) : '';
      console.log('presetcheck: import', ok ? 'ok' : 'REFUSED', text.length, '->', out.length, 'bytes');
      fetch('/preset-out', { method: 'POST', body: out });
    });
  }

  const about = { engine: 'libsumi ' + C.version(), midi: '—', frames: '' };
  const presetState = { name: '', pick: '' };
  const presetsAll = () => { try { return JSON.parse(lsGet(PRESETS_KEY) || '{}'); } catch { return {}; } };
  const download = (text, file) => {
    const a = document.createElement('a'); a.href = URL.createObjectURL(new Blob([text], { type: 'application/json' }));
    a.download = file; a.click(); setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  };
  let refreshAll = () => {};
  let printDiscarded = false;
  const actions = {
    dip: () => { C.dip(inst); printDiscarded = false; },
    clear: () => { C.dip(inst); printDiscarded = true; },   // the core still prints; the page never offers that one
    savePrint: () => savePrint(),
    // step 66: a recording from any shell, played here
    replayFile: () => {
      const inp = document.createElement('input'); inp.type = 'file'; inp.accept = '.sumireplay,text/plain';
      inp.onchange = () => { const f = inp.files && inp.files[0]; if (!f) return; f.text().then((t) => { if (!startReplay(t)) alert(`Not a midi-sink recording: ${f.name}`); }); };
      inp.click();
    },
    stopReplay: () => { if (replay && !replay.done) endReplay(); },
    reset: () => { try { localStorage.removeItem(SESSION_KEY); localStorage.removeItem(LEGACY_KEY); } catch {} location.reload(); },
    savePreset: () => {
      const n = presetState.name.trim(); if (!n) return;
      const all = presetsAll(); all[n] = sessionJSON(n); lsSet(PRESETS_KEY, JSON.stringify(all));
      presetState.pick = n; refreshAll();
    },
    loadPreset: () => { const t = presetsAll()[presetState.pick]; if (t && importJSON(t)) { readModel(); saveSession(); refreshAll(); } },
    deletePreset: () => { const all = presetsAll(); delete all[presetState.pick]; lsSet(PRESETS_KEY, JSON.stringify(all)); presetState.pick = ''; refreshAll(); },
    exportSession: () => { const n = presetState.name.trim() || presetState.pick || 'midi-sink session'; download(sessionJSON(n), n.replace(/[\/\\:"<>|?*]/g, '_') + '.json'); },
    importFile: () => {
      const inp = document.createElement('input'); inp.type = 'file'; inp.accept = '.json,application/json';
      inp.onchange = () => {
        const f = inp.files && inp.files[0]; if (!f) return;
        f.text().then((t) => {
          if (!importJSON(t)) { alert(`Not a midi-sink preset: ${f.name}`); return; }
          let n = ''; try { n = JSON.parse(t).name || ''; } catch {}
          if (!n || n === 'last session') n = f.name.replace(/\.json$/i, '');
          const all = presetsAll(); all[n] = sessionJSON(n); lsSet(PRESETS_KEY, JSON.stringify(all));
          presetState.pick = n; readModel(); saveSession(); refreshAll();
        });
      };
      inp.click();
    },
  };
  let gui = null;
  if (!EMBED) {
    gui = new GUI({ title: 'midi-sink', width: 320 });
    // Canvas first: the paper dip is the most-used control (#78). QOL §6: dip and clear, worded apart.
    const f4 = gui.addFolder('Canvas');
    f4.add(actions, 'dip').name('Dip the paper — keep the print');
    const printCtl = f4.add(actions, 'savePrint').name('Save the print as PNG').disable();
    f4.add(actions, 'clear').name('Clear the canvas — discard');
    f4.add(actions, 'replayFile').name('Replay a recording…');   // step 66
    f4.add(actions, 'stopReplay').name('Stop the replay');
    const f1 = gui.addFolder('Layout & look');
    f1.add(st, 'layout', LAYOUTS).name('Pitch layout').onChange(applySettings);
    // step 66: the instruments' rows (the iPad's picker, in the browser), the overlay, and the fingering as a controller
    // would send it — the valve CCs 110–112 and the slide's CC 113 straight into the engine; the overlay follows its state
    const arcCtl = f1.add(st, 'trumpetArc', { 'Column': 0, 'Arc': 1 }).name('Brass arrangement').onChange(applySettings);
    const tuningCtl = f1.add(st, 'stringTuning', { 'Standard guitar': 0, 'Whole-tone tap grid': 1, 'All fourths': 2 }).name('String tuning').onChange(applySettings);
    f1.add(ov, 'show').name('Layout overlay').onChange(() => { try { localStorage.setItem('sumi-web-overlay', ov.show ? '1' : '0'); } catch {} drawOverlay(true); });
    const fing = { v1: false, v2: false, v3: false, slide: 0 };
    const valveCtls = [1, 2, 3].map((k) => f1.add(fing, 'v' + k).name(`Valve ${k} (CC ${109 + k})`).onChange((on) => { C.midi(inst, 0xB0, 109 + k, on ? 127 : 0); }));
    const slideCtl = f1.add(fing, 'slide', 0, 1, 0.01).name('Slide (CC 113)').onChange((v) => { C.midi(inst, 0xB0, 113, Math.round(v * 127)); });
    layoutRows = () => {
      const l = Math.round(st.layout);
      arcCtl.show(l === 8 || l === 9); tuningCtl.show(l === 11);
      for (const c of valveCtls) c.show(l === 8);
      slideCtl.show(l === 9);
    };
    layoutRows();
    f1.add(st, 'viscosity', 0, 1, 0.01).name('Viscosity').onChange(applySettings);
    f1.add(st, 'inkFeed', 0.1, 4, 0.01).name('Ink feed (pressure)').onChange(applySettings);
    f1.add(st, 'fullRes').name('Full-resolution sim').onChange(applySettings);
    f1.add(st, 'bpm', 20, 300, 1).name('Tempo (rolls)').onChange(applySettings);
    f1.add(st, 'rollSpeed', 0.02, 0.25, 0.005).name('Roll speed').onChange(applySettings);

    // ---- the medium (MEDIUM §1) ----
    const fm = gui.addFolder('Medium');
    fm.add(st, 'medium', { 'Sumi — ink on washi': 0, 'Anod — strain-glow': 1 }).name('Medium').onChange(() => { applySettings(); rebuildMedium(); });
    const anodRows = [
      fm.add(st, 'anodDrop', 0.1, 1, 0.01).name('Strike charge (× drop)').onChange(applySettings),
      fm.add(st, 'anodGlow', 0.2, 5, 0.01).name('Glow scale').onChange(applySettings),
      fm.add(st, 'gridLines', 0, 256, 1).name('Grid lines (0–7 off)').onChange(applySettings),
    ];

    // ---- the substrate (QOL §2) ----
    const fs = gui.addFolder('Substrate');
    const sumiSub = [
      fs.addColor(st, 'tint').name('Paper tint').onChange(applySettings),
      fs.add(st, 'roughness', 0, 1, 0.01).name('Roughness').onChange(applySettings),
      fs.add(st, 'fiberScale', 0.5, 2, 0.01).name('Fiber scale').onChange(applySettings),
    ];
    const anodSub = [
      fs.add(st, 'anodDark', 0, 1, 0.01).name('Glass darkness').onChange(applySettings),
      fs.add(st, 'anodGrain', 0, 1, 0.01).name('Phosphor grain').onChange(applySettings),
      fs.add(st, 'anodBloom', 0, 3, 0.01).name('Glow bloom').onChange(applySettings),
      fs.add(st, 'anodBloomLevels', 1, 5, 1).name('Glow reach (octaves)').onChange(applySettings),
    ];
    fs.close();

    // ---- the palette (QOL §1): the medium's built-ins, the library, the custom slot's editor ----
    const fp = gui.addFolder('Palette');
    const pal = { lib: 0, count: 3, stops: [], gamma: 1, floor: 0, drift: 0, accent: { r: 0, g: 0, b: 0 }, clear: { r: 0, g: 0, b: 0 } };
    let activeCtl = null, libCtl = null, editor = null;
    const palFromCore = () => {
      const a = palRead();
      pal.count = Math.round(a[0]);
      pal.stops = [];
      for (let i = 0; i < 8; i++) pal.stops.push({ color: { r: lin2srgb(a[1 + 4 * i]), g: lin2srgb(a[2 + 4 * i]), b: lin2srgb(a[3 + 4 * i]) }, at: a[4 + 4 * i] });
      pal.gamma = Number(a[33].toPrecision(6)); pal.floor = Number(a[34].toPrecision(6)); pal.drift = Number(a[35].toPrecision(6));
      pal.accent = { r: lin2srgb(a[36]), g: lin2srgb(a[37]), b: lin2srgb(a[38]) };
      pal.clear = { r: lin2srgb(a[39]), g: lin2srgb(a[40]), b: lin2srgb(a[41]) };
    };
    const palToCore = () => {
      const a = new Array(45).fill(0);
      a[0] = pal.count;
      pal.stops.forEach((s, i) => { a[1 + 4 * i] = srgb2lin(s.color.r); a[2 + 4 * i] = srgb2lin(s.color.g); a[3 + 4 * i] = srgb2lin(s.color.b); a[4 + 4 * i] = i === 0 ? 0 : i === pal.count - 1 ? 1 : s.at; });
      for (let i = pal.count; i < 8; i++) { a[1 + 4 * i] = a[1 + 4 * (pal.count - 1)]; a[2 + 4 * i] = a[2 + 4 * (pal.count - 1)]; a[3 + 4 * i] = a[3 + 4 * (pal.count - 1)]; a[4 + 4 * i] = 1; }
      a[33] = pal.gamma; a[34] = pal.floor; a[35] = pal.drift;
      a[36] = srgb2lin(pal.accent.r); a[37] = srgb2lin(pal.accent.g); a[38] = srgb2lin(pal.accent.b);
      a[39] = srgb2lin(pal.clear.r); a[40] = srgb2lin(pal.clear.g); a[41] = srgb2lin(pal.clear.b);
      palWrite(a); saveSession();
    };
    const rebuildEditor = () => {
      if (editor) editor.destroy();
      editor = null;
      if (st.palette !== 3) return;
      palFromCore();
      const anod = st.medium === 1;
      editor = fp.addFolder(anod ? 'Custom — dim to burning' : 'Custom — thin to pooled');
      let shown = pal.count;
      editor.add(pal, 'count', 2, 8, 1).name('Stops').onChange(() => {
        // the desktop's rule: a new stop goes in before the last, midway between its neighbours;
        // removing takes out the one before the last
        const want = Math.round(pal.count), stops = pal.stops.slice(0, shown);
        while (stops.length < want) {
          const last = stops[stops.length - 1], prev = stops[stops.length - 2];
          const avg = (k) => lin2srgb((srgb2lin(prev.color[k]) + srgb2lin(last.color[k])) / 2);   // in linear light, as the desktop does
          const mid = { color: { r: avg('r'), g: avg('g'), b: avg('b') }, at: (prev.at + 1) / 2 };
          stops.splice(stops.length - 1, 0, mid);
        }
        while (stops.length > want) stops.splice(stops.length - 2, 1);
        while (stops.length < 8) stops.push({ color: { ...stops[stops.length - 1].color }, at: 1 });
        pal.stops = stops; pal.count = want; shown = want;
        palToCore(); rebuildEditor();
      });
      for (let i = 0; i < pal.count; i++) {
        const lab = i === 0 ? (anod ? 'Dim' : 'Thin') : i === pal.count - 1 ? (anod ? 'Burning' : 'Pooled') : `Stop ${i + 1}`;
        editor.addColor(pal.stops[i], 'color').name(lab).onChange(palToCore);
        if (i > 0 && i < pal.count - 1) editor.add(pal.stops[i], 'at', 0, 1, 0.01).name(`  at`).onChange(palToCore);
      }
      editor.add(pal, 'gamma', 0.25, 4, 0.01).name('Depth curve').onChange(palToCore);
      editor.add(pal, 'floor', 0, 1, 0.01).name('Depth floor').onChange(palToCore);
      editor.add(pal, 'drift', 0, 1, 0.01).name('Hue drift').onChange(palToCore);
      editor.addColor(pal, 'accent').name(anod ? 'Drift toward (halo)' : 'Drift toward').onChange(palToCore);
      if (!anod) editor.addColor(pal, 'clear').name('Clear water').onChange(palToCore);
    };
    const rebuildPalette = () => {
      const medium = st.medium === 1 ? 1 : 0;
      const names = libNames(medium);
      const opts = {}; names.slice(0, 3).forEach((n, i) => { opts[n] = i; }); opts['Custom'] = 3;
      // rebuilt in order each time (lil-gui appends): Active, Library, Load, then the editor
      for (const c of [activeCtl, libCtl, loadCtl]) if (c) c.destroy();
      if (editor) { editor.destroy(); editor = null; }
      activeCtl = fp.add(st, 'palette', opts).name('Active').onChange(() => { applySettings(); rebuildEditor(); });
      const libOpts = {}; names.forEach((n, i) => { libOpts[n] = i; });
      if (pal.lib >= names.length) pal.lib = 0;
      libCtl = fp.add(pal, 'lib', libOpts).name('Library');
      loadCtl = fp.add({ load: () => {
        const e = libPalette(st.medium === 1 ? 1 : 0, pal.lib); if (!e) return;
        palWrite(e.floats); st.palette = 3; applySettings(); rebuildPalette();
      } }, 'load').name('Load into custom');
      rebuildEditor();
    };
    let loadCtl = null;
    fp.close();

    const rebuildMedium = () => {
      const anod = st.medium === 1;
      for (const c of anodRows) c.show(anod);
      for (const c of sumiSub) c.show(!anod);
      for (const c of anodSub) c.show(anod);
      rebuildPalette();
    };

    // ---- presets (QOL §3) ----
    const fr = gui.addFolder('Presets');
    fr.add(presetState, 'name').name('Name');
    fr.add(actions, 'savePreset').name('Save as preset');
    let pickCtl = null;
    const rebuildPresets = () => {
      const names = Object.keys(presetsAll()).sort();
      if (pickCtl) pickCtl.destroy();
      if (!names.includes(presetState.pick)) presetState.pick = names[0] || '';
      pickCtl = fr.add(presetState, 'pick', names.length ? names : ['(none saved)']).name('Saved');
    };
    const loadBtn = fr.add(actions, 'loadPreset').name('Load');
    const delBtn = fr.add(actions, 'deletePreset').name('Delete');
    fr.add(actions, 'exportSession').name('Export this session (JSON)');
    fr.add(actions, 'importFile').name('Import a preset…');
    fr.close();

    const f2 = gui.addFolder('Expression routing');
    f2.add(st, 'inputMode', { MPE: 1, 'Classic keyboard': 2, Wind: 3 }).name('Input').onChange(applySettings);
    // 1.1.0 (MEDIUM §4): every mode has the medium's default first — the desktop's lists.
    f2.add(st, 'bend', { 'Medium default': 255, 'Glide (drag the drop)': 0, 'Ripple amplitude': 1, 'Torsion wavelength': 2, 'Spark frequency': 3, 'Chladni stir': 4 }).name('Per-note bend').onChange(applySettings);
    f2.add(st, 'press', { 'Medium default': 255, 'Ink feed': 0, 'Lamb–Oseen swirl': 1, 'Torsion sweep feed': 2 }).name('Channel pressure').onChange(applySettings);
    f2.add(st, 'slide', { 'Medium default': 255, Hue: 0, Pinch: 1, 'Spark frequency': 2 }).name('Slide (CC 74)').onChange(applySettings);
    f2.add(st, 'pinchVariant', { Saddle: 0, 'Crossed tines': 1 }).name('Pinch style').onChange(applySettings);
    f2.add(st, 'vortexProfile', { Exponential: 0, Rankine: 1, Torsion: 3 }).name('Vortex profile').onChange(applySettings);
    f2.add(st, 'torsionSweep', { Off: 0, On: 1 }).name('Torsion sweep on note-on').onChange(applySettings);
    f2.add(st, 'wakeProfile', { 'Inviscid doublet': 0, 'Viscous stroke': 1 }).name('Stylus wake').onChange(applySettings);
    f2.add(st, 'wakeSpread', 1.5, 12, 0.1).name('Spread (l/a)').onChange(applySettings);
    f2.close();
    const f3 = gui.addFolder('Ripple');
    f3.add(st, 'rippleAmount', 0, 127, 1).name('Amount (CC 102)').onChange(applySettings);
    f3.add(st, 'rippleWavelength', 0, 127, 1).name('Wavelength (CC 103)').onChange(applySettings);
    f3.add(st, 'rippleAngle', 0, 180, 1).name('Angle (deg)').onChange(applySettings);
    f3.close();
    const f5 = gui.addFolder('About');
    f5.add(about, 'engine').name('Engine').disable();
    const midiCtl = f5.add(about, 'midi').name('MIDI inputs').disable();
    const framesCtl = f5.add(about, 'frames').name('Frame').disable();
    f5.add(actions, 'reset').name('Reset settings');
    f5.close();
    refreshAll = () => {
      gui.controllersRecursive().forEach((c) => c.updateDisplay());
      rebuildMedium(); rebuildPresets();
      const has = Object.keys(presetsAll()).length > 0;
      loadBtn.enable(has); delBtn.enable(has);
    };
    refreshAll();
    if (window.innerWidth < 720) gui.close();
    window.__sumiGuiRefresh = () => { about.midi = midiText; midiCtl.updateDisplay(); };
    window.__sumiGuiFrame = (text, ready) => {
      about.frames = text; framesCtl.updateDisplay();
      if (ready && !printDiscarded) printCtl.enable(); else printCtl.disable();
    };
  }

  // ---- scene API ----
  const frameWaiters = [];
  const api = {
    drop: (x, y, r, l) => C.drop(inst, x, y, r, l),
    // step 67 (DECISIONS_9 #6): the MEDIUM'S strike — a drop under Sumi, the spark on its charge under Anod, as a
    // finger's tap is in the app (sumi_gesture_tap); the scenes' strikes go through it, their clear drops stay drops
    tap: (x, y, r) => C.gTap(inst, x, y, r),
    tine: (x0, y0, x1, y1, a, z) => C.tine(inst, x0, y0, x1, y1, a, z),
    vortex: (x, y, s, r, p) => C.vortex(inst, x, y, s, r, p),
    wake: (x0, y0, x1, y1, a) => C.wake(inst, x0, y0, x1, y1, a),
    pinch: (x, y, k, ang) => C.pinch(inst, x, y, k, ang),
    chladni: (psi, balance, sx, x0, sy, y0) => C.chladni(inst, psi, balance, sx, x0, sy, y0),
    burst: (x, y, a, D, theta0, m) => C.burst(inst, x, y, a, D, theta0, m),
    spark: (x, y, r, D, theta0, layer) => C.spark(inst, x, y, r, D, theta0, layer),
    sparkShear: (x, y, band, A, B, k, phase, theta0) => C.sparkShear(inst, x, y, band, A, B, k, phase, theta0),
    chirikov: (x, y, K, periods, eps, phase) => C.chirikov(inst, x, y, K, periods, eps, phase),
    midi: (s, d1, d2) => C.midi(inst, s, d1, d2),
    mapCC: (cc, target) => C.mapCC(inst, 0xFF, cc, target),
    param: (name) => C.getParam(inst, PARAM_ID[name]),
    setParam: (name, v) => C.setParam(inst, PARAM_ID[name], v),
    aspect,
    frames: (n) => new Promise((res) => frameWaiters.push({ n, res })),
    // The layout probe (the tablets' hit-test): the cell under (x, y) on the
    // current layout — scenes use it to put a VOICE where they want a picture.
    probe: (x, y) => {
      const out = M._malloc(20);   // step 66: five floats (the flags came)
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
      // step 66: a replay's dips leave their prints pending, and a pending print owns the readback machinery
      // (the renderer's rule) — the page consumes them first, as the desktop's ledger would
      if (dump.drainPrints) {
        const wp = M._malloc(4), hp = M._malloc(4);
        while (C.readPrint(inst, 0, 0, wp, hp)) {
          const w = M.HEAPU32[wp >> 2], h = M.HEAPU32[hp >> 2], bytes = w * h * 4, buf = M._malloc(bytes);
          const got = C.readPrint(inst, buf, bytes, wp, hp); M._free(buf);
          console.log('fielddump: a pending print consumed', w, h, got ? '' : '(read failed)');
          if (!got) break;
        }
        M._free(wp); M._free(hp);
      }
      const ok = C.fieldBegin(inst);
      if (ok || dump.frames % 30 === 0) console.log('fielddump: readback begin ->', ok, 'frame', dump.frames);
      if (ok) dump.stage = 2;
      else if (dump.frames > (dump.patience || 30)) { console.error('fielddump: readback never started'); dump.stage = 4; }
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

  // ---- Phase 9 step 66 (QOL §1; DECISIONS_8 #26): REPLAY PLAYBACK — the gallery's "watch it again" ----
  // A .sumireplay from any shell, played on the scripted clock: one update at the recorded dt and one render per
  // recorded frame, as many per animation frame as the wall clock asks (every remaining frame at once under
  // ?pace=0, the gate's way); the live input and the settings muted meanwhile; the recording's palette, the
  // viewer's canvas size (the gate holds the canvas at the recording's size and reads the field after).
  const startReplay = (text, { lab = false, wallHz = 0 } = {}) => {
    if (replay && !replay.done) endReplay();
    const n = M.lengthBytesUTF8(text) + 1; const p = M._malloc(n); M.stringToUTF8(text, p, n);
    const ok = C.rOpen(p, n - 1); M._free(p);
    if (!ok) { status('not a midi-sink recording'); console.error('replay: not a replay file'); return false; }
    if (wallHz > 0) { C.rRebucket(1 / wallHz); console.log('replay: NEGATIVE - re-bucketed by wall time at', wallHz, 'Hz ->', C.rStat(0), 'frames'); }
    let flags = 2;   // SUMI_REPLAY_APPLY_PALETTE: the recording's look; the size is the viewer's canvas
    if (lab) {       // the gate: SUMI_REPLAY_APPLY_SIZE, the canvas held at the recording's size
      const w = C.rStat(4), h = C.rStat(5), pr = C.rStat(6) || 1;
      replayLab = { w, h, pr };
      canvas.width = w; canvas.height = h; canvas.style.width = `${Math.round(w / pr)}px`; canvas.style.height = `${Math.round(h / pr)}px`;
      overlayCanvas.width = w; overlayCanvas.height = h; overlayCanvas.style.width = canvas.style.width; overlayCanvas.style.height = canvas.style.height;
      flags |= 1;
    }
    C.rBegin(inst, flags);
    replay = { acc: 0, flags, done: false };
    const banner = C.rBanner();
    status(banner); document.title = 'midi-sink — replay';
    console.log('replay: begin', banner, '|', C.rStat(0), 'frames,', C.rStat(1).toFixed(2), 's,', C.rStat(7), 'events');
    if (gui) gui.close();
    readModel(); drawOverlay(true);
    return true;
  };
  const endReplay = () => {
    if (!replay || replay.done) return;
    replay.done = true;
    console.log('replay: done', C.rStat(2), 'frames,', C.rStat(3).toFixed(2), 's on the scripted clock');
    C.rClose();
    status('Replay finished');
    document.title = 'midi-sink — replay done';
    if (REPLAYDUMP) dump = { stage: 1, frames: 100, drainPrints: true, patience: 900 };   // the field as the last frame left it: stepDump's readback from the next frame (the dips' prints consumed first)
    else if (POST) { try { navigator.sendBeacon('/scene', 'replay'); } catch {} }
    if (!replayLab) { readModel(); refreshAll(); drawOverlay(true); }
  };
  if (REPLAY) {
    fetch(REPLAY).then((r) => { if (!r.ok) throw new Error(`${r.status} ${r.statusText}`); return r.text(); })
      .then((text) => { if (!startReplay(text, { lab: REPLAYDUMP, wallHz: REPLAYWALL })) throw new Error('not a replay'); })
      .catch((e) => { console.error('replay:', e.message); status('replay: ' + e.message); });
  }

  // ---- frame loop ----
  let last = performance.now(), frames = 0, firstMarked = false, fpsAcc = 0, fpsN = 0;
  const loop = (now) => {
    const dt = Math.min(0.1, Math.max(0, (now - last) / 1000)); last = now;
    if (replay && !replay.done) {
      // step 66: the replay drives the clock — its frames, each an update at the recorded dt and a render, as many as the
      // wall clock asks (every remaining one under pace=0); nothing due: re-composite only (the field stays)
      replay.acc += dt; if (replay.acc > 0.25) replay.acc = 0.25;
      let n = 0;
      const cap = PACE0 ? 40 : 8;   // pace=0: forty recorded frames a tick (a 20-s recording in a second), never one task for all
      while (n < cap && (PACE0 || replay.acc > 0)) {
        const fdt = C.rStep(inst, replay.flags);
        if (fdt < 0) { endReplay(); break; }
        C.update(inst, fdt); C.render(inst); replay.acc -= fdt; n++;
      }
      if (replay && !replay.done) {
        if (n === 0) C.render(inst);
        if (C.rStat(8) <= 0) endReplay();
        else if (frames % 15 === 0) status(`${C.rBanner()} · ${C.rStat(3).toFixed(1)} / ${C.rStat(1).toFixed(1)} s`);
      }
    } else {
      pressureTick(dt);    // v0.6 pressure gesture (#49)
      C.update(inst, dt);
      C.render(inst);
    }
    frames++;
    if ((frames & 7) === 0) drawOverlay(false);   // step 66: the brass fingering may have moved (a controller's CCs)
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
  drawOverlay(true);   // step 66
  requestAnimationFrame(loop);
  if (scene) runScene();
}

main().catch((e) => { console.error(e); status('error: ' + e.message); });
