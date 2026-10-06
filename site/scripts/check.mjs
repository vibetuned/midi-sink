/**
 * Guard the site against drift. Runs after every build (`npm run check`):
 *
 *  1. Internal links resolve to a page in dist/.
 *  2. NO SECOND IMPLEMENTATION (Step 26 DONE): every operator demo is the
 *     release wasm reached through the embed API. The built site must contain
 *     no wasm, no sumi.js/sumi-host.js, no WebGPU shader source; every
 *     <iframe> on an operator page must point at the marble app with a
 *     `scene=` query; and every one of the seventeen scenes — the 1.0 nine
 *     and the Phase-6 six (torsion, chladni, burst, spark, chirikov, anod),
 *     whose pages landed at step 67 — is embedded somewhere. The one other
 *     iframe target is the Suzu lab, `<marble>suzu/<page>.html` (the synth
 *     compiled to wasm, composed beside the marble app — DECISIONS_9 #2):
 *     every lab page the web build serves must be embedded once, through
 *     <Lab>, and nothing else may be framed.
 *  3. The store-required pages exist at their frozen URLs: /, /privacy/,
 *     /support/ (DECISIONS_4 #22) — the lanes and store listings hardcode them.
 *  4. The gallery manifest parses and every entry carries the caption fields
 *     (title, device, layout, modes) — the gallery doubles as an index into
 *     the user guide, so a captionless video is a broken entry.
 *  5. KaTeX rendered: no unrendered `$$` blocks survive in the HTML.
 *
 * Exit code 1 on any problem.
 */
import { readdirSync, readFileSync, existsSync, statSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..");
const dist = join(root, "dist");
const contentDir = join(root, "src/content/docs");
if (!existsSync(dist)) { console.error("dist/ not found — run `npm run build` first."); process.exit(1); }

// dist/marble is the wasm app composed in for `npm run preview` (and by the
// pages workflow) — it is the thing the docs embed, not part of the docs, so
// every rule below skips it.
const walk = (dir, out = []) => {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory() && p === join(dist, "marble")) continue;
    e.isDirectory() ? walk(p, out) : out.push(p);
  }
  return out;
};
const problems = [];
const base = (process.env.DOCS_BASE ?? "/").replace(/\/$/, "");
const marble = (process.env.PUBLIC_MARBLE_URL ?? "/marble/").replace(/\/?$/, "/");

// --- 1. internal links -----------------------------------------------------
// Links under the embed root (PUBLIC_MARBLE_URL) and under the stable app's root are composed by the
// workflow; a lab link must use the embed root.
const html = walk(dist).filter((f) => f.endsWith(".html"));
// Inline scripts hold template literals (the gallery's runtime cards) — markup,
// not links; strip them before scanning, and keep them aside for rule 2.
const scriptsOf = (src) => [...src.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script>/g)].map((m) => m[1]).join("\n");
const markupOf = (src) => src.replace(/<script\b[^>]*>[\s\S]*?<\/script>/g, "");
for (const file of html) {
  const src = markupOf(readFileSync(file, "utf8"));
  for (const m of src.matchAll(/href="([^"#?]+)(?:[#?][^"]*)?"/g)) {
    const href = m[1];
    if (/^(https?:|mailto:|data:|\/\/)/.test(href)) continue;
    if (href.startsWith(marble)) continue; // the marble app is composed in by the workflow, not built here
    // The stable app's root is always composed too (a product link: the home's hero, the install page) — but
    // a LAB link must follow the embed root (<LabLink>), since the stable may predate the lab (DECISIONS_9 #2).
    if (href.startsWith(`${base}/marble/`)) {
      if (/\/marble\/suzu\//.test(href)) problems.push(`lab link ${href} is not under the embed root ${marble} (use <LabLink>) in ${relative(dist, file)}`);
      continue;
    }
    let target;
    if (href.startsWith("/")) {
      const stripped = base && href.startsWith(`${base}/`) ? href.slice(base.length) : href;
      target = join(dist, stripped);
    } else target = resolve(dirname(file), href);
    const ok = [target, join(target, "index.html"), `${target}.html`].some((c) => existsSync(c) && statSync(c).isFile());
    if (!ok) problems.push(`dead link ${href} in ${relative(dist, file)}`);
  }
}

// --- 2. no second implementation -------------------------------------------
const all = walk(dist);
for (const f of all) {
  const name = f.split("/").pop();
  if (/\.wasm$/.test(name) || /^sumi(-host)?\.js$/.test(name)) problems.push(`engine artifact inside the docs build: ${relative(dist, f)}`);
}
const SCENES = ["drop", "feed", "tine", "vortex", "rankine", "wake", "viscous", "pinch", "ripple", "lamb_oseen", "scroll",
  // Step 44b served them (web/site/scenes.js); step 67 wrote their pages: every one is required now.
  "torsion", "chladni", "burst", "spark", "chirikov", "anod"];
const KNOWN_SCENES = SCENES;
// Step 67: the Suzu lab's pages (web/suzu/site/<page>.html, index aside), embedded through <Lab>.
const LAB_PAGES = ["cell", "modal", "strings", "chaos", "flute", "winds"];
const seen = new Set();
const seenLab = new Set();
for (const file of html) {
  const raw = readFileSync(file, "utf8");
  const src = markupOf(raw);
  for (const m of src.matchAll(/<iframe[^>]*\ssrc="([^"]+)"/g)) {
    const s = m[1].replace(/&amp;/g, "&");   // attribute values are HTML-escaped
    if (s.startsWith("https://www.youtube-nocookie.com/")) continue; // gallery embeds (runtime only, never in HTML — belt and braces)
    if (!s.startsWith(marble)) { problems.push(`iframe not pointing at the marble app (${s}) in ${relative(dist, file)}`); continue; }
    if (s.startsWith(`${marble}suzu/`)) {   // a Suzu lab panel: <marble>suzu/<page>.html, no query
      const page = s.slice(`${marble}suzu/`.length).replace(/\.html(\?.*)?$/, "");
      if (!LAB_PAGES.includes(page)) problems.push(`unknown Suzu lab page "${page}" in ${relative(dist, file)}`);
      else seenLab.add(page);
      continue;
    }
    const q = new URL(s, "http://x").searchParams;
    if (!q.get("scene")) problems.push(`operator iframe without scene= in ${relative(dist, file)}`);
    else if (!KNOWN_SCENES.includes(q.get("scene"))) problems.push(`unknown scene "${q.get("scene")}" in ${relative(dist, file)}`);
    else seen.add(q.get("scene"));
    if (q.get("embed") !== "1") problems.push(`operator iframe without embed=1 in ${relative(dist, file)}`);
  }
  // Prose may NAME these (the design notes do); only executable script may not contain them.
  if (/navigator\.gpu|requestAdapter\(|createSumi\(/.test(scriptsOf(raw))) problems.push(`WebGPU/engine code inside a docs page: ${relative(dist, file)}`);
}
for (const s of SCENES) if (!seen.has(s)) problems.push(`scene "${s}" is never embedded — every operator needs its live demo`);
for (const p of LAB_PAGES) if (!seenLab.has(p)) problems.push(`Suzu lab page "${p}" is never embedded — every voice family needs its live panel`);
// The lab list IS the web build's: a page added to web/suzu/site must be named here and embedded.
{
  const labDir = join(root, "..", "web", "suzu", "site");
  if (existsSync(labDir)) {
    const served = readdirSync(labDir).filter((f) => f.endsWith(".html") && f !== "index.html").map((f) => f.replace(/\.html$/, ""));
    for (const n of served) if (!LAB_PAGES.includes(n)) problems.push(`lab page "${n}" is served by web/suzu/site but unknown to check.mjs`);
    for (const n of LAB_PAGES) if (!served.includes(n)) problems.push(`lab page "${n}" is known to check.mjs but not served by web/suzu/site`);
  }
}
// Step 44b: the known list IS the marble app's — a scene added to web/site/scenes.js must be named here.
{
  const scenesJs = join(root, "..", "web", "site", "scenes.js");
  if (existsSync(scenesJs)) {
    const served = [...readFileSync(scenesJs, "utf8").matchAll(/^  ([a-z_]+): \{/gm)].map((m) => m[1]);
    for (const n of served) if (!KNOWN_SCENES.includes(n)) problems.push(`scene "${n}" is served by web/site/scenes.js but unknown to check.mjs`);
    for (const n of KNOWN_SCENES) if (!served.includes(n)) problems.push(`scene "${n}" is known to check.mjs but not served by web/site/scenes.js`);
  }
}

// --- 3. frozen URLs ----------------------------------------------------------
for (const p of ["index.html", "privacy/index.html", "support/index.html"]) {
  if (!existsSync(join(dist, p))) problems.push(`frozen URL missing: /${p.replace(/index\.html$/, "")}`);
}

// --- 4. gallery manifest ----------------------------------------------------
{
  const mf = join(root, "public/gallery/gallery.json");
  try {
    const data = JSON.parse(readFileSync(mf, "utf8"));
    if (!Array.isArray(data.performances)) problems.push("gallery.json: `performances` must be an array");
    else data.performances.forEach((e, i) => {
      for (const k of ["title", "device", "layout", "modes"]) if (e[k] == null) problems.push(`gallery.json[${i}] missing "${k}"`);
      if (e.modes && !Array.isArray(e.modes)) problems.push(`gallery.json[${i}] "modes" must be an array`);
      if (e.src && e.youtube) problems.push(`gallery.json[${i}] has both src and youtube`);
      if (e.guide && !/^(\.\.?\/|https?:)/.test(e.guide) && !existsSync(join(dist, e.guide.replace(/^\//, ""), "index.html"))) problems.push(`gallery.json[${i}] guide link ${e.guide} does not resolve`);
      // Step 67: a card's replay is a .sumireplay under /gallery/ (the marble app's ?replay= fetches it). A missing
      // file is a pending recording, reported, not a failure — the author drops the files in.
      if (e.replay != null) {
        if (!/^\/gallery\/[^/]+\.sumireplay$/.test(e.replay)) problems.push(`gallery.json[${i}] replay must be /gallery/<name>.sumireplay (got ${e.replay})`);
        else if (!existsSync(join(dist, e.replay.replace(/^\//, "")))) console.warn(`  note: gallery.json[${i}] replay ${e.replay} is not in the tree yet (recording pending)`);
      }
    });
  } catch (e) { problems.push(`gallery.json unreadable: ${e.message}`); }
}

// --- 5. math rendered --------------------------------------------------------
for (const file of html) {
  const src = readFileSync(file, "utf8");
  if (/<p>\s*\$\$/.test(src) || /\$\$\s*<\/p>/.test(src)) problems.push(`unrendered $$ math in ${relative(dist, file)}`);
}

// --- report ------------------------------------------------------------------
const pages = walk(contentDir).filter((f) => /\.mdx?$/.test(f)).length;
console.log(`checked ${html.length} built pages from ${pages} sources; ${seen.size}/${SCENES.length} scenes and ${seenLab.size}/${LAB_PAGES.length} Suzu lab pages embedded via ${marble}`);
for (const p of problems) console.error(`  error: ${p}`);
if (problems.length) { console.error(`\n${problems.length} problem(s).`); process.exit(1); }
console.log("ok.");
