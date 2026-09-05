# Evidence — Step 26: Documentation site

PHASE5 §6 (five books + the chart + citations); ROADMAP_4 Step 26. Decisions:
`_work/DECISIONS_4.md` #22–#28. Machine: the author's Mac (Node 26 / npm 11,
Astro 7.3, Starlight 0.42, KaTeX 0.18 via `@astrojs/markdown-remark`,
Chrome 152 for the captures). No core change; no Step-25 host change beyond
the gate tool growing `--fullshots` page paths and `--window`.

## What landed

* **`site/`** — an Astro Starlight site, the author's house style (battuta,
  Midi Stroke), standalone package with its own lockfile. 37 pages built from
  36 sources:
  * **User guide** (10): install, Marble mode, Play mode, layouts, the stylus,
    the control strip, devices (ROLI Piano / Airwave / Osmose / wind /
    classic), the desktop app, the web canvas, paper and prints.
  * **The Operators** (9 + intro): drop, tine, vortex (both profiles), wake,
    pinch, ripple, Lamb–Oseen swirl, scroll — each with the formula (KaTeX),
    invariants, ownership rule, and a **live demo = the release wasm through
    `<Operator scene=…>`** (an iframe of `/marble/?scene=…&embed=1`). The
    wake and swirl pages carry their one-line Jaffer-lineage notes.
  * **Architecture** (6): Option 2, the C-ABI, threading, orientation,
    swapchain ownership per backend, hostmpe.
  * **Performance gallery**: runtime manifest (`public/gallery/gallery.json`,
    schema in its README) — three planned performances, marked pending; the
    **Jaffer tribute identified and its piece named**.
  * **Design notes + changelog**: GENERATED verbatim by
    `scripts/build-notes.mjs` from `docs/CHANGELOG.md`, `docs/DECISIONS.md`
    (Parts I–III) and `_work/DECISIONS_4.md` (Part IV) — gitignored,
    rebuilt before every build.
  * **Reference**: the MIDI implementation chart (rendered from
    `src/data/midi-chart.json`), citations & acknowledgments.
  * **Store pages at frozen URLs**: `/privacy/`, `/support/`, `/` (#22).
* **`tools/chart_check.py`** — replays the Play-mode byte logs against the
  chart JSON: present/absent/undocumented per source and channel class, plus
  the constants.
* **`site/scripts/check.mjs`** — post-build drift check: dead links, no
  second operator implementation (no wasm / engine JS / WebGPU code in the
  docs; every iframe → marble app with a known `scene=` and `embed=1`; all 9
  scenes embedded), the three frozen URLs, gallery manifest captions,
  rendered math.
* **Spine**: the `web` job in `release.yml` now builds the site (Node 22,
  `SITE_VERSION` from the tag, `PUBLIC_MARBLE_URL=/marble/`) and composes
  docs at the Pages root + wasm under `/marble/` — the root placeholder is
  gone. `build.yml` gained a `docs` job (build + drift check + chart check).

## DONE verification

| DONE criterion | Result | Evidence |
|---|---|---|
| Every operator demo is the release wasm via the embed API — no second implementation | **PASS** — `check.mjs`: "9/9 scenes embedded via /marble/", zero engine artifacts in `dist/`, every iframe `scene=…&embed=1`; a grep of the built site finds `sumi.wasm`/`createSumi` only as prose in the published decision log | `site_check.txt`, `site_operator_wake_live_embed.png` (the wake page with the WebGPU engine rendering inside the docs page), `site_operators_index_live_embed.png` |
| The chart matches the byte logs | **PASS** — 32 asserts, 0 failed, over four Step-22 sessions (fingers, stylus, strip + pen pedal, session config; restored from git history into `bytelogs/`): every documented output observed, every forbidden one absent (0 finger CC 74, 0 pen 0xA0), no undocumented output in any source, MCM = 15 members, RPN 0 = 48 on 15 distinct channels, first finger bend = centre 8192, strip on the master only | `chart_check.txt`, `bytelogs/` |
| Gallery plays with correct captions; tribute video in place with piece identified and PD verified | **PARTIAL — author input outstanding.** The gallery renders from the manifest with device / layout / modes captions on every card (validated by `check.mjs`); no performance is recorded yet, so all three cards read *recording pending* (the step ships with whatever is recorded, per the roadmap). **Piece identified from Jaffer's own video credits:** *Bouquet* → Kendime, *Latte* → Ali Paşa, *Wave* → Rampi Rampi (his page names none of them; the title cards do). **Chosen: Ali Paşa** (the single-stylus *Latte*). PD basis: `C: Trad.` / `O: Turkey` in his ABC; SFDH: anonymous lament for Ali Pasha of Van, Turkish Folk Music Archive no. 398 (M. Sarısözen); the dance was set by Bora Özkök and is not what is performed (#27) | `jaffer_credit_{bouquet,latte,wave}.png`, `site_gallery.png` |
| All citations resolve | **PASS** — every citation checked against its source while writing: arXiv 1702.02106 (Jaffer 2017) and 1810.04646 (Jaffer 2018) abstract pages; IEEE CG&A 32(6):26–35, 2012, DOI 10.1109/MCG.2011.51; the CSAIL marbling page and its stroke/vortex/transfer PDFs; Lamb 6th ed. 1932 CUP / Dover 1945; Rankine, Griffin, London 1858; MPE v1.1 M1-100-UM 14 Apr 2022 superseding RP-053. `check.mjs` verifies the internal links; external ones were fetched | `reference/citations` page; `site_check.txt` |
| Privacy / support / homepage URLs live and recorded | **AUTHORED; live on the first tagged deploy.** `/`, `/privacy/`, `/support/` exist in `dist/` (asserted by `check.mjs`) and are frozen in DECISIONS_4 #22 with the domain `midi-sink.vibetuned.com`. Going live needs two human actions: the Pages custom domain in the repo settings and a DNS CNAME to `vibetuned.github.io` | `site_home.png`, `site_check.txt` |
| Site deploys from the same tag as the artifacts | **PASS (authored; runs on your tag)** — the docs build lives inside the release `web` job; `SITE_VERSION` = the tag; the composed Pages tree is asserted (`index.html`, `privacy/`, `support/`, `marble/sumi.wasm`) before upload. No push-to-main deploy exists, deliberately (#22) | `.github/workflows/release.yml` |
| Gallery accepts additions without a site rebuild | **PASS** — the page fetches `gallery/gallery.json` at runtime; adding a performance is an entry + a file | `site/public/gallery/README.md` |

## Scene refinement after the author's review (DECISIONS_4 #29, #30)

The first cut showed each operator's result. On review ("we don't see the
math taking effect, only the result"), the scenes were reworked in the web
host (`web/site/scenes.js`; the probe exposed through the shim as
`sumi_web_probe` — existing ABI, no core change):

* **Paced** — a `⏱` slider (frames per step, default 2, 0 = instant); the
  exactly-composing operators run as n small passes (tine z/n, vortex A/n,
  pinch and wake sub-steps one per step, the ripple amount ramped in), which
  is also the composition invariant shown live.
  `scene_vortex_paced_early.png` / `scene_vortex_paced_late.png` — the same
  scene at 1.5 s and 7 s with pace 6.
* **Two sites** — every scene works on two ring clusters (top-left,
  bottom-right): horizontal vs vertical tine, +A/−A vortices, fold axes θ and
  θ + 90° — `site_operator_{tine_two_sites,vortex_two_sites,pinch_two_axes}.png`.
  The swirl is the exception (author review: stirring a cluster about its own
  centre shows little): one centre voice, four ring pools in the corners, a
  long stir — the 1/r² far field carries the pools into commas around the
  core: `site_operator_swirl_far_field.png`.
* **New `feed` scene** — a re-struck note (rings) beside a note held under
  channel pressure (one growing band), on the drop page:
  `site_operator_drop_and_feed.png`. 10/10 scenes in the sweep
  (`scenes_sweep.txt`), field gate unchanged (PASS), drift check 10/10 scenes
  embedded (`site_check.txt`).
* The requested Marble-mode gesture for feed/swirl is **not taken**: it needs
  new core ABI (no gesture entry point for either; a Marble touch has no note
  to synthesize MIDI with) and the core is frozen this phase; Shift + drag is
  also already the pinch. Mapping recommended for Phase 6 in #30.

## Captures

`site_home.png` (splash), `site_guide_play_mode.png`, `site_midi_chart.png`,
`site_gallery.png` — 900×700 headless; `site_operator_wake_live_embed.png`
and `site_operators_index_live_embed.png` — 1280×2600, the docs page with the
live WebGPU embed rendering inside its iframe (captured through the DevTools
protocol from a composed docs + `/marble/` tree, the exact layout the
workflow deploys). `jaffer_credit_*.png` — the title cards of Jaffer's three
animations naming their tunes.

## Found on the way

Astro 7 replaced its Markdown processor: remark/rehype plugins need
`@astrojs/markdown-remark` installed (#23). Starlight ≥ 0.39 dropped
`label` + `autogenerate` on one object (wrap the autogenerate in `items`).
YAML frontmatter needs quoting when a description contains `: `. The first
chart-check run failed on the checker, not the chart: a re-syncing session
repeats RPN 0 on every member, so the assertion counts distinct channels.
The drift check's first run had three false positives (script template
literals read as hrefs; prose naming `navigator.gpu` in the published
decision log; HTML-escaped `&amp;` in iframe queries) — fixed in the checker.
Chrome's `--screenshot` has no WebGPU, so the gate tool's `--fullshots` mode
now also takes page paths and a `--window` size for docs captures.

## Not taken

The optional Turkish-moiré scripted scene (a new scene in the Step-25 host
for an optional bridge — #27). Videos: author input, pending.
