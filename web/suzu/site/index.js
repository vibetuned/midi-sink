// index.js — the Suzu lab's front page (Phase 8 step 59c, DECISIONS_7 #31): a card for every panel.
import { PAGES, navigation, $ } from './lab-core.js';

navigation('index');
$('cards').innerHTML = PAGES.map((p) => `<a class="card" href="${p.href}"><h2>${p.title}</h2><p>${p.blurb}</p></a>`).join('');
