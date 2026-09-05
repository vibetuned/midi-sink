/**
 * Publish the engineering record. Runs before every build (`npm run notes`):
 *
 *   docs/CHANGELOG.md           -> src/content/docs/notes/changelog.md
 *   docs/DECISIONS.md           -> src/content/docs/notes/decisions/part-{1,2,3}.md
 *   _work/DECISIONS_4.md        -> src/content/docs/notes/decisions/part-4.md
 *                                  (while Phase 5 is in flight; it merges into
 *                                  DECISIONS.md as Part IV when the phase ships,
 *                                  and this script then finds it there instead)
 *
 * "Lightly edited" per PHASE5 §6: entries are kept VERBATIM. The only edits
 * are mechanical — a Starlight frontmatter block, the top-level heading
 * demoted so the page title is not repeated, and repository-internal path
 * prefixes (`_work/`, `docs/`) trimmed to the bare file name, because the
 * reader of the site has no working tree. Nothing is reworded.
 */
import { readFileSync, writeFileSync, mkdirSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const repo = join(here, "..", "..");
const out = join(here, "..", "src/content/docs/notes");
mkdirSync(join(out, "decisions"), { recursive: true });

const trimPaths = (s) =>
  s
    .replace(/`_work\/([A-Za-z0-9_.-]+)`/g, "`$1`")
    .replace(/`docs\/([A-Za-z0-9_.-]+)`/g, "`$1`")
    .replace(/\b_work\/(DECISIONS_\d|PHASE\d_SPEC|ROADMAP_\d)\.md\b/g, "$1.md")
    .replace(/\bdocs\/(DECISIONS|CHANGELOG|ROADMAP|PROJECT_SPEC)\.md\b/g, "$1.md");

// Astro/MDX-safe: these pages are plain Markdown (.md), so braces and angle
// brackets in the engineering prose are literal — no escaping needed.
const page = (title, description, body, extra = "") =>
  `---\ntitle: "${title.replace(/"/g, '\\"')}"\ndescription: "${description.replace(/"/g, '\\"')}"\n${extra}---\n\n` +
  `<div class="notes-verbatim">\n\n${body.trim()}\n\n</div>\n`;

// ---- changelog ---------------------------------------------------------
{
  const src = trimPaths(readFileSync(join(repo, "docs/CHANGELOG.md"), "utf8"));
  const body = src.replace(/^# Changelog\s*\n/, "");
  writeFileSync(
    join(out, "changelog.md"),
    page("Changelog", "Every release of midi-sink, condensed from the per-step DONE evidence.", body, "sidebar:\n  order: 3\n")
  );
}

// ---- decisions ---------------------------------------------------------
const parts = [];
{
  const src = trimPaths(readFileSync(join(repo, "docs/DECISIONS.md"), "utf8"));
  // Split on the "# Part N — …" headings; the preamble before Part I is dropped
  // (it explains the file layout of the repository, not the decisions).
  const re = /^# (Part [IV]+ — [^\n]+)$/gm;
  const heads = [...src.matchAll(re)];
  heads.forEach((m, i) => {
    const start = m.index + m[0].length;
    const end = i + 1 < heads.length ? heads[i + 1].index : src.length;
    parts.push({ title: m[1], body: src.slice(start, end) });
  });
  if (!parts.length) throw new Error("no '# Part …' headings found in docs/DECISIONS.md");
}
const partIV = join(repo, "_work/DECISIONS_4.md");
if (existsSync(partIV) && !parts.some((p) => /Part IV/.test(p.title))) {
  const src = trimPaths(readFileSync(partIV, "utf8"));
  const body = src.replace(/^# [^\n]+\n/, "");
  parts.push({ title: "Part IV — Phase 5: Packaging, Release, Web & Documentation (steps 23–33)", body });
}
parts.forEach((p, i) => {
  const n = i + 1;
  // Demote "## Step N" to "## Step N" (kept) — Starlight's page title is the H1.
  writeFileSync(
    join(out, "decisions", `part-${n}.md`),
    page(
      `Design notes — ${p.title}`,
      "The decision log, published verbatim: every spec ambiguity resolved during implementation and why.",
      p.body,
      `sidebar:\n  order: ${n}\n  label: "${p.title.replace(/ — .*$/, "")}"\n`
    )
  );
});
console.log(`notes: changelog + ${parts.length} decision parts written to ${out}`);
