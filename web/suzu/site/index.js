// index.js — the Suzu lab's front page (Phase 8 step 59c, DECISIONS_7 #31): a card for every panel.
import { PAGES, navigation, $ } from './lab-core.js';

navigation('index');
$('cards').innerHTML = PAGES.map((p) => `<a class="card" href="${p.href}"><h2>${p.title}</h2><p>${p.blurb}</p></a>`).join('');

const VCSL_PRESETS = [
  { name: 'Dan Tranh', tag: 'Plucked Zither', href: 'modal.html?preset=dan_tranh', blurb: '13 modes on a steel wire lattice with inharmonicity B = 1.32e-4 and T60 = 3.44 s.' },
  { name: 'Glockenspiel', tag: 'Tuned Metallophone', href: 'modal.html?preset=glockenspiel', blurb: '3 modes of a stiff steel bar with T60 = 4.16 s and hard mallet strike profile.' },
  { name: 'Tubular Bells', tag: 'Orchestral Chimes', href: 'modal.html?preset=tubular_bells', blurb: '9 modal partials with long 17.36 s ring-down and prominent strike tone.' },
  { name: 'Concert Harp', tag: 'Pedal Harp', href: 'modal.html?preset=concert_harp', blurb: '5 modes plucked at 0.445 with warm T60 = 5.73 s decay.' },
  { name: 'Baroque Recorder', tag: 'Fipple Flute', href: 'flute.html?preset=recorder', blurb: '128-node acoustic bore with fitted jet delay τ = 0.50 and wall loss T60 = 2.0 s.' },
  { name: 'Tenor Saxophone', tag: 'Single Reed / Cone', href: 'winds.html?preset=tenor_sax', blurb: 'Truncated conical bore with single cane reed resonance at 11.5 kHz (Q 0.72).' },
];

const vcslEl = $('vcsl-cards');
if (vcslEl) {
  vcslEl.innerHTML = VCSL_PRESETS.map((p) =>
    `<a class="card" href="${p.href}"><h2>${p.name} <span style="font-size:12px; color:var(--muted); font-weight:normal;">— ${p.tag}</span></h2><p>${p.blurb}</p></a>`
  ).join('');
}

