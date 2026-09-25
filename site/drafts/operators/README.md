# Operator-page drafts (Phase 6) — land in `site/src/content/docs/operators/` at step 63

Drafted in each step's evidence folder (removed from the tree at the Phase-6
close, 2026-09-26; git history keeps the folders) and parked here so step 63
finds them. Outside `src/`, so Astro does not build them. They describe the
operators as their steps shipped them — check each against `docs/DECISIONS.md`
Part V before it goes live, in particular:

| Draft | From | Superseded by |
|---|---|---|
| `torsion.mdx` | step 36 | — |
| `chladni.mdx` | step 37 (the Taylor–Green cellular flow) | **`chladni_cells.mdx`** (the rework before step 43: the cells are the eddies, `DECISIONS_5 #59–#62`) — the physics paragraph of the first draft may survive |
| `burst.mdx`, `burst_literature.md` | step 38 | the strike composition changed twice (#71, #88): the burst is back in the Anod strike with `burst_order`; the author's note is to be trimmed and signed |
| `spark.mdx` | step 39 | the composed strike as #88 ships it |
| `chirikov.mdx` | step 40 | the ceiling 1.25 (#42); the author's feel still open (#89) |
| `anod.mdx` | step 42 | the water grid (#57), the bloom (#69), the binding table (#70–#72, #88) |
| `palettes.mdx` | step 43 | — |

The scenes the pages embed (`torsion`, `chladni`, `burst`, `spark`,
`chirikov`, `anod`) are served by `web/site/scenes.js` and known to
`site/scripts/check.mjs`, which requires them to be embedded once these
pages exist.
