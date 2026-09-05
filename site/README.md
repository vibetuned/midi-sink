# site — the midi-sink documentation

An [Astro Starlight](https://starlight.astro.build/) site (the author's house
style — battuta and Midi Stroke use the same) with KaTeX for the formulae.
Standalone: its own `package.json` and lockfile.

```sh
cd site
npm install
npm run dev        # renders the notes pages, then serves at the printed URL
npm run build      # notes → static site in dist/ → drift check
npm run preview    # serve dist/
python3 ../tools/chart_check.py   # the MIDI chart vs the byte logs
```

Deployed by `.github/workflows/release.yml` (the `web` lane) to
**https://midi-sink.vibetuned.com/** from the release tag, with the marble
web app composed next to it at `/marble/`. Same tag, same job, one deploy.
For a GitHub Pages project site: `DOCS_BASE=/midi-sink npm run build`.

## Five books and a chart

| Book | Where | Notes |
|---|---|---|
| User guide | `src/content/docs/guide/` | install, both modes, layouts, stylus, strip, devices, desktop, web, paper & prints |
| The Operators | `src/content/docs/operators/` | one `.mdx` per deformation; every demo is `<Operator scene=…>` — an iframe of the release wasm through the scene API. **No second implementation**, enforced by `scripts/check.mjs`. |
| Architecture | `src/content/docs/architecture/` | the Option-2 pattern for host builders |
| Performance gallery | `gallery.mdx` + `public/gallery/gallery.json` | rendered at runtime from the manifest; add a recording = add an entry (schema in `public/gallery/README.md`) |
| Design notes & changelog | **generated** into `src/content/docs/notes/` by `scripts/build-notes.mjs` from `docs/CHANGELOG.md`, `docs/DECISIONS.md` and `_work/DECISIONS_4.md` — verbatim, paths trimmed; gitignored |
| MIDI implementation chart | `reference/midi-chart.mdx` renders `src/data/midi-chart.json` through `<Chart>`; `tools/chart_check.py` verifies that JSON against the Play-mode byte logs in `docs/evidence/step26/bytelogs/` |

Store-required pages at frozen URLs: `/privacy/`, `/support/`, and the
homepage `/` (DECISIONS_4 #22).

## Live demos locally

`<Operator>` points at `PUBLIC_MARBLE_URL` (default `/marble/`). **`npm run
preview` composes the wasm build into `dist/marble/` first** (the layout the
workflow deploys), so with a `build-web` in the repo root every demo runs:

```sh
emcmake cmake -B ../build-web -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build ../build-web
npm run build && npm run preview          # http://localhost:4321/ — demos live
npm run preview:stop                      # stop a preview left running (another terminal)
npm run preview:clean                     # …and remove the composed dist/marble/
```

(`MARBLE_DIST=…` points at another wasm build.) For the dev server, which has
no `dist/`, serve the wasm build and point the embeds at it:

```sh
python3 ../tools/web_serve.py                       # http://localhost:8765/
PUBLIC_MARBLE_URL=http://localhost:8765/ npm run dev
```

## The drift check (`npm run check`, runs post-build)

Dead internal links · no wasm / engine JS / WebGPU code in the docs build ·
every `<iframe>` targets the marble app with a valid `scene=` and `embed=1` ·
all nine scenes embedded somewhere · `/`, `/privacy/`, `/support/` exist ·
`gallery.json` parses with captions on every entry · no unrendered `$$`.

## Adding a page

Add the `.md`/`.mdx` under `src/content/docs/`, list its slug in the sidebar
in `astro.config.mjs`, write links **relative** so a base path survives. Math
is `$…$` / `$$…$$`; in `.mdx` files keep braces out of prose (or use `.md`).
