/**
 * Compose the marble web app into dist/marble/ for a local preview — the same
 * layout the pages workflow deploys (docs at /, the Step-25 wasm at
 * /marble/), so every <Operator> embed resolves. Runs before `npm run preview`.
 *
 *   MARBLE_DIST=../build-web/web-dist   (default) — build it with
 *   emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-web
 *
 * The Suzu lab (build-web/suzu-dist, SUZU_DIST to override) is composed at
 * dist/marble/suzu/ beside it — the <Lab> embeds (step 67, DECISIONS_9 #2).
 *
 * The copy is NOT part of the docs build: scripts/check.mjs ignores dist/marble
 * (the engine may not live inside the docs) and `astro build` recreates dist/.
 */
import { cpSync, existsSync, rmSync, mkdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const dist = join(here, "..", "dist");
const src = resolve(here, "..", process.env.MARBLE_DIST ?? "../build-web/web-dist");

if (!existsSync(dist)) { console.error("dist/ not found — run `npm run build` first."); process.exit(1); }
if (!existsSync(join(src, "sumi.wasm"))) {
  console.warn(`compose: no wasm build at ${src} — the operator demos will 404 in the preview.`);
  console.warn("  build it: emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-web");
  process.exit(0);
}
const target = join(dist, "marble");
rmSync(target, { recursive: true, force: true });
mkdirSync(target, { recursive: true });
cpSync(src, target, { recursive: true });
console.log(`compose: ${src} -> dist/marble/  (the live demos now resolve in the preview)`);
const lab = resolve(here, "..", process.env.SUZU_DIST ?? "../build-web/suzu-dist");
if (existsSync(join(lab, "suzu.wasm"))) {
  cpSync(lab, join(target, "suzu"), { recursive: true });
  console.log(`compose: ${lab} -> dist/marble/suzu/  (the Suzu lab panels now resolve in the preview)`);
} else {
  console.warn(`compose: no Suzu lab build at ${lab} — the lab panels will 404 in the preview (the web build makes it).`);
}
