// winds.js — the Suzu lab's saxophone and trumpet (Phase 8 step 59c, DECISIONS_7 #31; SYNTH §2.11, §2.12, §5).
//
// Voice kinds 7 (a reed on a cone) and 8 (lips on a flared bore). The bore and its envelope from the inspection (the
// shared BoreView), the valve drawn at the blown end from its opening and flow, the valve's portrait from the ring's aux
// channels (y, v), the mouth-power ledger from the inspection (k[8] the mouth's work, k[9] the energy held), the
// trumpet's registers from CC 74. ?gate=winds checks the sax on its note with the ledger holding, the trumpet's three
// registers, and the naive junction refused — and, the gate bypassed, blown up and caught.
import { QS, $, css, noteName, noteHz, clamp, Lab, bindPower, ensureStarted, showNote, buildKeys, markKeys, bindKeymap, drawSpectrum,
  Portrait, Strip, banner, footer, navigation, offlineRun, at, pitchAt, gateGuard, BoreView, register, signed } from './lab-core.js';

const P_REF = 0.005;
const PATCH = { 7: { voice_kind: 7 }, 8: { voice_kind: 8 } };
const NOTES = {
  7: 'A cone closed at a truncated apex, its mouthpiece holding the missing tip\'s volume. Along a cone the pressure grows toward the apex as one over the distance; it is the pressure times the distance that stands as a sine, so that is what the tube shows: one half of a sine on the note. At the left the reed against the mouthpiece\'s lay — the mouth\'s pressure closes it, the bore\'s swing opens it, and the air passes where it is open.',
  8: 'A cylinder that flares into a bell, the note on the bore\'s third peak. At the left the lips: an outward valve, pushed open by the mouth and pulled shut by the bore. The embouchure (CC 74) tunes the lips — loosen them and the trumpet falls to the peak below, tighten and it climbs the staircase to the octave.',
};
const lab = new Lab({ params: { ...PATCH[7] }, traceMask: (1 << 7) | (1 << 8), traceDecim: 2, recentPoints: 1023, envelope: true, snapHz: 60 });
const S = { kind: 7, ins: null, envRms: null, pts: [], ys: 1e-9, vs: 1e-9, qs: 1e-9, hs: 1e-9, breath: 70, lips: 64, ramp: null, hz: 0, hMean: null, hSerial: -1 };

// ---- the browser check ----
async function gate() {
  const note = 57, f0 = noteHz(note), sr = 48000, checks = [];
  const s = await offlineRun({ params: { ...PATCH[7] }, script: [[0, 0xB1, 2, 70], [0, 0x91, note, 100]], seconds: 1.2, envelope: false, snapHz: 20 });
  const cs = [0.6, 0.8, 1.0].map((t) => register(pitchAt(s.x, sr, t), f0));
  const led = s.snaps.filter((q) => q.inspect.length && q.inspect[0].k[8] > 0).map((q) => q.inspect[0].k[9] / q.inspect[0].k[8]);
  checks.push({ name: 'the sax sounds its note in the first register (A3, breath 70)', pass: cs.every((r) => r && r.reg === 1 && Math.abs(r.cents) <= 40), detail: cs.map((r) => r ? `${r.reg}×${signed(r.cents, 1)}` : '—').join(', ') + ' cents at 0.6, 0.8, 1.0 s' });
  checks.push({ name: 'the ledger holds: the energy held never exceeds the mouth\'s work', pass: led.length > 10 && Math.max(...led) <= 1, detail: `at worst ${Math.max(...led).toFixed(3)} of the work over ${led.length} snapshots` });
  const regs = [];
  for (const cc of [0, 64, 127]) {
    const t = await offlineRun({ params: { ...PATCH[8] }, script: [[0, 0xB1, 74, cc], [0, 0xB1, 2, 70], [0, 0x91, note, 100]], seconds: 1.0, envelope: false });
    const hz = pitchAt(t.x, sr, 0.9); regs.push(hz > 0 ? 1200 * Math.log2(hz / f0) : NaN);
  }
  checks.push({ name: 'the trumpet\'s registers: CC 74 at 0 the peak below, at 64 the note, at 127 the octave', pass: regs[0] < -500 && Math.abs(regs[1]) <= 40 && regs[2] > 900, detail: `${regs.map((c) => Number.isFinite(c) ? signed(c) : '—').join(', ')} cents from the note` });
  const naive = { ...PATCH[7], valve_naive: 1, bore_loss: 0 };
  const refused = await offlineRun({ params: naive, script: [], seconds: 0.05 });
  checks.push({ name: 'the valve gate refuses the naive junction and says why', pass: refused.ready && !refused.ready.applied && /junction/.test(refused.ready.log), detail: refused.ready ? refused.ready.log.trim().slice(0, 170) : 'no answer' });
  const bend = 8192 + Math.round(4 / 48 * 8191);
  const blown = await offlineRun({ params: { ...naive, valve_gate: 0 }, script: [[0, 0xB1, 2, 70], [0, 0x91, note, 100], [at(sr, 0.3), 0xE1, bend & 127, bend >> 7]], seconds: 1.5, envelope: false });
  checks.push({ name: 'with the gate bypassed the naive junction blows up on a transient — and the lab catches it (the red control)', pass: blown.ready?.applied && blown.blowups.length > 0, detail: blown.blowups.length ? `caught at ${(blown.blowups[0].block * 128 / sr).toFixed(2)} s, after the bend at 0.30 s; the engine restarted ${blown.restarts.length}×` : 'not caught' });
  return { checks, data: { regs } };
}

// ---- the live page ----
function sendBreath(v) { S.breath = v; $('breath').value = v; $('breath-out') ; $('breath').nextElementSibling.value = v; lab.cc(2, v); }
async function hold(note) { await ensureStarted(lab); lab.cc(2, S.breath); if (S.kind === 8) lab.cc(74, S.lips); lab.hold(note); markKeys($('keys'), note, true); }
function release() { lab.release(); stopTimers(); markKeys($('keys'), 0, false); }
function stopTimers() { if (S.ramp) { clearInterval(S.ramp); S.ramp = null; $('ramp').textContent = 'Ramp the breath'; $('sweep').textContent = 'Sweep the embouchure'; } }
async function ramp(kind) {
  if (S.ramp) { stopTimers(); return; }
  if (!lab.held) await hold(lab.note === 60 ? 57 : lab.note);
  const t0 = performance.now(), dur = kind === 'lips' ? 16000 : 12000;
  $(kind === 'lips' ? 'sweep' : 'ramp').textContent = 'Stop';
  S.ramp = setInterval(() => {
    const v = Math.min(127, Math.round(127 * (performance.now() - t0) / dur));
    if (kind === 'lips') { S.lips = v; $('lips').value = v; $('lips').nextElementSibling.value = v; lab.cc(74, v); } else sendBreath(v);
    if (v >= 127) stopTimers();
  }, 30);
}
async function apply(values) { const r = await lab.setParams(values); if (!r.ok) banner('refused', `The engine refused the patch: ${r.log.trim()}`); else if (!$('vgate').checked) banner(); return r; }
const SAX_PRESETS = {
  default: {
    voice_kind: 7,
    level: 0.25,
    attack_s: 0.1,
    release_s: 0.25,
    cutoff_hz: 20000.0,
    resonance: 0.0,
    press_blows: 1,
    reed_hz: 12000.0,
    reed_q: 0.8,
    reed_open: 0.6,
    reed_close: 3.0,
    reed_area: 0.15,
    reed_noise: 0.02,
    cone_apex: 0.3,
    hint: 'Click a key to hold the note; the breath slider is the mouth. Keys also on the keyboard: A W S E D F T G Y H U J K.'
  },
  vcsl_tenor: {
    voice_kind: 7,
    level: 0.25,
    attack_s: 0.174,
    release_s: 0.25,
    cutoff_hz: 20000.0,
    resonance: 0.0,
    press_blows: 1,
    reed_hz: 11500.0,
    reed_q: 0.72,
    reed_open: 0.5,
    reed_close: 3.0,
    reed_area: 0.14,
    reed_noise: 0.02,
    cone_apex: 0.25,
    hint: 'Tenor Saxophone (VCSL fitted) — single cane reed on truncated cone: fitted reed resonance 11.5 kHz (Q 0.72), resting gap 0.50, cone apex ratio 0.25.'
  }
};

async function setKind(k) {
  S.kind = k; S.ys = S.vs = S.qs = S.hs = 1e-9;
  for (const b of $('kind').querySelectorAll('button')) b.classList.toggle('on', +b.dataset.k === k);
  for (const el of document.querySelectorAll('.only-7')) el.hidden = k !== 7;
  for (const el of document.querySelectorAll('.only-8')) el.hidden = k !== 8;
  $('bore-note').textContent = NOTES[k];
  $('valve-title').textContent = k === 7 ? 'The reed\'s portrait' : 'The lips\' portrait';
  const saxKey = ($('sax-preset') && $('sax-preset').value) || 'default';
  const saxCfg = SAX_PRESETS[saxKey] || SAX_PRESETS.default;
  const p = k === 7 ? { ...saxCfg } : { ...PATCH[k] };
  const hintText = p.hint;
  delete p.hint;
  if ($('naive').checked) { p.valve_naive = 1; p.bore_loss = 0; p.valve_gate = $('vgate').checked ? 0 : 1; } else { p.valve_naive = 0; p.bore_loss = 0.3; p.valve_gate = 1; }
  showNote('Calibrating the embouchure — the engine measures how the valve pulls the pitch, a moment\'s pause.');
  const r = await apply(p);
  showNote(hintText || 'Click a key to hold the note; the breath slider is the mouth. Keys also on the keyboard: A W S E D F T G Y H U J K.');
  if (r.ok && lab.held) hold(lab.note);
}

// the valve at the blown end: the reed against the lay (the sax), the lips (the trumpet); the opening from the
// inspection (k[2], the rest opening k[6]), the flow (k[5]) as the stream's width
function drawValve(g, ins, endW, yc, R, narrow) {
  g.fillStyle = css('--muted'); g.font = '11px system-ui, sans-serif'; g.textAlign = 'center';
  const h = ins ? ins.k[2] : 0, h0 = ins ? ins.k[6] : 1, q = ins ? ins.k[5] : 0;
  if (ins) { S.hs = Math.max(Math.abs(h), S.hs * 0.995, h0); S.qs = Math.max(Math.abs(q), S.qs * 0.995, 1e-9); }
  const gap = ins ? 2 + 18 * clamp(h / S.hs, 0, 1) : 10, tipX = endW * 0.62;
  if (S.kind === 7) {
    g.strokeStyle = css('--tube'); g.lineWidth = 2;
    g.beginPath(); g.moveTo(8, yc - 6); g.lineTo(tipX, yc - 4); g.lineTo(endW, yc - R * 0.35); g.stroke();   // the lay and the mouthpiece's roof
    g.strokeStyle = css('--accent'); g.lineWidth = 3;
    g.beginPath(); g.moveTo(8, yc + 14); g.quadraticCurveTo(tipX * 0.6, yc + 10, tipX, yc - 4 + gap); g.stroke();   // the reed
    g.strokeStyle = css('--tube'); g.lineWidth = 2; g.beginPath(); g.moveTo(tipX, yc + R * 0.35 + 6); g.lineTo(endW, yc + R * 0.35); g.stroke();
    g.fillText('the reed', tipX * 0.5, yc + 30);
  } else {
    g.fillStyle = css('--accent');
    g.beginPath(); g.ellipse(tipX, yc - gap / 2 - 9, 16, 9, 0, 0, 2 * Math.PI); g.fill();
    g.beginPath(); g.ellipse(tipX, yc + gap / 2 + 9, 16, 9, 0, 0, 2 * Math.PI); g.fill();
    g.strokeStyle = css('--tube'); g.lineWidth = 2;
    g.beginPath(); g.moveTo(tipX + 18, yc - R * 0.6); g.quadraticCurveTo(endW - 10, yc - R * 0.6, endW, yc - R * 0.3); g.moveTo(tipX + 18, yc + R * 0.6); g.quadraticCurveTo(endW - 10, yc + R * 0.6, endW, yc + R * 0.3); g.stroke();   // the cup
    g.fillStyle = css('--muted'); g.fillText('the lips', tipX, yc + R * 0.6 + 26);
  }
  if (ins && q > 0) {   // the air through the opening
    const wq = 1 + 6 * clamp(q / S.qs, 0, 1);
    g.strokeStyle = css('--u'); g.globalAlpha = 0.8; g.lineWidth = wq; g.lineCap = 'round';
    g.beginPath(); g.moveTo(2, yc - (S.kind === 7 ? 0 : 0)); g.lineTo(endW - 4, yc); g.stroke(); g.globalAlpha = 1;
  }
  g.fillStyle = css('--muted'); g.textAlign = 'left'; g.fillText('the mouth', 2, yc - R * 0.6 - 8);
}

function wire() {
  navigation('winds');
  const bore = new BoreView($('bore'), { pRef: P_REF, leftLabel: 'the mouthpiece', rightLabel: 'the bell' });
  const valve = new Portrait($('valve')), ledger = new Strip($('ledger'), 12), hist = new Strip($('history'), 16);
  lab.on('ready', () => {
    $('engine-line').textContent = `Voxo ${lab.version}, the winds, ${lab.sampleRate} Hz`;
    if (QS.has('preset')) {
      const qp = QS.get('preset');
      if (qp === 'tenor' || qp === 'tenor_sax' || qp === 'vcsl_tenor') {
        $('sax-preset').value = 'vcsl_tenor';
        setKind(7);
      }
    } else if (QS.has('demo')) {
      if (QS.get('demo') === 'trumpet') setKind(8).then(() => hold(57)); else hold(57);
    }
  });
  lab.on('snap', (m) => {
    S.ins = m.inspect.find((v) => v.held) || m.inspect[m.inspect.length - 1] || null;
    S.envRms = m.env;
    // the opening's mean: one snapshot catches the valve anywhere in its cycle (the lips shut for part of it), sixty a second average it
    if (S.ins && S.ins.serial === S.hSerial) S.hMean += 0.05 * (S.ins.k[2] - S.hMean);
    else if (S.ins) { S.hMean = S.ins.k[2]; S.hSerial = S.ins.serial; } else { S.hMean = null; S.hSerial = -1; }
    if (m.recent && m.recent.length >= 8 && S.ins) {
      const r = m.recent, n = r.length / 4, per = Math.round(2 * S.ins.rate2 / 2 / Math.max(S.ins.freq, 1)), from = Math.max(0, n - per), pts = [];
      for (let i = from; i < n; i++) { S.ys = Math.max(S.ys * 0.999, Math.abs(r[4 * i + 2])); S.vs = Math.max(S.vs * 0.999, Math.abs(r[4 * i + 3])); }
      for (let i = from; i < n; i++) pts.push([r[4 * i + 2] / S.ys, r[4 * i + 3] / S.vs]);
      S.pts = pts;
    } else if (!S.ins) S.pts = [];
  });
  lab.on('blowup', (m) => banner('red', `It blew up ${(m.block * 128 / lab.sampleRate).toFixed(2)} s into the engine's life: the naive junction let the valve and the bore make energy the mouth never gave — the ledger's bound crossed — until the samples were no longer numbers. The lab muted it and restarted the engine; the valve gate refuses this patch for exactly this reason.`));
  buildKeys($('keys'), { lo: 48, hi: 84, onDown: (m) => hold(m) });
  bindKeymap(60, (m) => hold(m));
  bindPower(lab, $('power'));
  $('volume').addEventListener('input', (e) => lab.gain(+e.target.value));
  $('sax-preset').addEventListener('change', () => { if (S.kind === 7) setKind(7); });
  $('breath').nextElementSibling.value = S.breath; $('lips').nextElementSibling.value = S.lips;
  for (const b of $('kind').querySelectorAll('button')) b.addEventListener('click', () => setKind(+b.dataset.k));
  $('breath').addEventListener('input', (e) => { stopTimers(); sendBreath(+e.target.value); });
  $('lips').addEventListener('input', (e) => { stopTimers(); S.lips = +e.target.value; e.target.nextElementSibling.value = S.lips; lab.cc(74, S.lips); });
  $('ramp').addEventListener('click', () => ramp('breath'));
  $('sweep').addEventListener('click', () => ramp('lips'));
  $('stop').addEventListener('click', release);
  $('naive').addEventListener('change', () => setKind(S.kind));
  $('vgate').addEventListener('change', () => { setKind(S.kind); if ($('vgate').checked && $('naive').checked) banner('red', 'The valve gate is bypassed with the naive junction on a lossless bore. Hold a note, then bend it: the ledger\'s line will be crossed.'); });
  $('bend').addEventListener('click', () => { lab.bend(5); setTimeout(() => lab.bend(0), 600); });
  setKind(7);
  const tick = () => {
    lab.readAudio(); S.hz = lab.pitch();
    const f0 = noteHz(lab.note), ins = S.ins, reg = register(S.hz, f0);
    bore.cone = S.kind === 7;
    bore.draw(ins, S.envRms, drawValve, lab.ctx ? 'Hold a note — the bore appears when it sounds.' : 'Start the sound, then hold a note.');
    valve.draw([S.pts], { fade: 0.12, fixed: 1.05, xLabel: 'y', yLabel: 'v' });
    const work = ins ? ins.k[8] : 0, held = ins ? ins.k[9] : 0;
    $('m-work').style.width = work > 0 ? '100%' : '0%'; $('m-held').style.width = work > 0 ? `${clamp(100 * held / work, 0, 100)}%` : '0%';
    $('o-work').textContent = work > 0 ? work.toPrecision(3) : '—'; $('o-held').textContent = work > 0 ? `${(100 * held / work).toFixed(1)} %` : '—';
    ledger.push({ r: work > 0 ? 10 * Math.log10(Math.max(held / work, 1e-6)) : null });
    ledger.draw([{ key: 'r', color: css('--trace'), lo: -40, hi: 3, width: 2 }], [[0, 'the bound: all the work held', 'r'], [-20, '1 %', 'r']]);
    $('r-note').textContent = `${noteName(lab.note)} · ${f0.toFixed(1)} Hz`;
    $('r-hz').textContent = reg ? `${S.hz.toFixed(1)} Hz` : (lab.held ? 'silent' : '—');
    $('r-reg').textContent = reg ? reg.name : '—';
    $('r-cents').textContent = reg ? `${signed(1200 * Math.log2(S.hz / f0))} cents` : '—';
    $('r-pm').textContent = ins ? `${(ins.k[1] / P_REF).toFixed(2)} P_ref` : '—';
    $('r-h').textContent = ins && S.hMean !== null ? `${(S.hMean / Math.max(ins.k[6], 1e-9)).toFixed(2)} × at rest` : '—';
    hist.push({ cents: S.hz > 0 ? 1200 * Math.log2(S.hz / f0) : null });
    const grid = S.kind === 8 ? [[-702, 'peak below', 'cents'], [0, 'the note', 'cents'], [498, '', 'cents'], [884, '', 'cents'], [1200, 'the octave', 'cents']] : [[0, 'the note', 'cents'], [1200, 'the octave', 'cents']];
    hist.draw([{ key: 'cents', color: css('--trace'), lo: -1000, hi: 1500, width: 2 }], grid);
    drawSpectrum($('spectrum'), lab, { f0, sounding: S.hz });
    footer(lab, S.kind === 7 ? 'saxophone' : 'trumpet');
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
  if (QS.has('demo')) ensureStarted(lab);
}

if (QS.get('gate') === 'winds') gateGuard('winds', gate); else wire();
