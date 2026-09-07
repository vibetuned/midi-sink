// @ts-check
import { defineConfig } from "astro/config";
import starlight from "@astrojs/starlight";
import remarkMath from "remark-math";
import rehypeKatex from "rehype-katex";

/**
 * The midi-sink documentation site (Phase 5, Step 26).
 *
 * Deployed by .github/workflows/release.yml (the `web` lane) to GitHub Pages
 * at https://midi-sink.vibetuned.com — the site owns the domain root and the
 * marble web app (the Step-25 wasm) is composed next to it under /marble/.
 * The custom domain is set in the repo's Pages settings, with a DNS CNAME
 * midi-sink.vibetuned.com -> vibetuned.github.io (the battuta / midi-stroke
 * pattern). For a GitHub Pages PROJECT site instead:
 *
 *   DOCS_BASE=/midi-sink npm run build
 *
 * Every internal link in the content is RELATIVE and every embed goes through
 * <Operator>, which reads PUBLIC_MARBLE_URL (default "/marble/"), so both
 * layouts work without touching the pages. The version shown in the footer
 * comes from SITE_VERSION (the release workflow's tag) — never hand-edited.
 *
 * Stable URLs (DECISIONS_4 #22 — hardcoded by every release lane and store
 * listing, frozen here): /  /privacy/  /support/  /marble/
 */
const SITE = process.env.SITE_URL ?? "https://midi-sink.vibetuned.com";

export default defineConfig({
  site: SITE,
  base: process.env.DOCS_BASE ?? "/",
  markdown: {
    remarkPlugins: [remarkMath],
    rehypePlugins: [rehypeKatex],
  },
  integrations: [
    starlight({
      title: "midi-sink",
      description:
        "midi-sink — a suminagashi ink-marbling instrument driven by expressive MIDI: user guide, the operator book with live demos, architecture, gallery, design notes and the MIDI implementation chart.",
      logo: { src: "./src/assets/logo.png", alt: "midi-sink" },
      favicon: "/favicon-32.png",
      head: [
        { tag: "link", attrs: { rel: "apple-touch-icon", href: "/favicon-180.png" } },
        { tag: "meta", attrs: { name: "theme-color", content: "#1c1b19" } },
        { tag: "meta", attrs: { property: "og:image", content: `${SITE}/og.png` } },
        { tag: "meta", attrs: { name: "twitter:card", content: "summary_large_image" } },
      ],
      social: [{ icon: "github", label: "GitHub", href: "https://github.com/vibetuned/midi-sink" }],
      editLink: { baseUrl: "https://github.com/vibetuned/midi-sink/edit/main/site/" },
      lastUpdated: true,
      tableOfContents: { minHeadingLevel: 2, maxHeadingLevel: 3 },
      customCss: ["katex/dist/katex.min.css", "./src/styles/custom.css"],
      components: { Footer: "./src/components/Footer.astro" },
      sidebar: [
        {
          label: "User guide",
          items: [
            { slug: "guide/install" },
            { slug: "guide/marble-mode" },
            { slug: "guide/play-mode" },
            { slug: "guide/layouts" },
            { slug: "guide/stylus" },
            { slug: "guide/control-strip" },
            { slug: "guide/devices" },
            { slug: "guide/desktop" },
            { slug: "guide/web" },
            { slug: "guide/paper-and-prints" },
          ],
        },
        {
          label: "The Operators",
          items: [
            { slug: "operators" },
            { slug: "operators/drop" },
            { slug: "operators/tine" },
            { slug: "operators/vortex" },
            { slug: "operators/wake" },
            { slug: "operators/pinch" },
            { slug: "operators/ripple" },
            { slug: "operators/swirl" },
            { slug: "operators/scroll" },
          ],
        },
        {
          label: "Architecture",
          items: [
            { slug: "architecture" },
            { slug: "architecture/c-abi" },
            { slug: "architecture/threading" },
            { slug: "architecture/orientation" },
            { slug: "architecture/swapchains" },
            { slug: "architecture/hostmpe" },
          ],
        },
        {
          label: "Performance gallery",
          items: [{ slug: "gallery" }],
        },
        {
          label: "Reference",
          items: [
            { slug: "reference/settings" },
            { slug: "reference/midi-chart" },
            { slug: "reference/citations" },
            { slug: "notes/changelog" },
            {
              label: "Design notes",
              collapsed: true,
              items: [{ autogenerate: { directory: "notes/decisions" } }],
            },
          ],
        },
        {
          label: "Project",
          items: [{ slug: "privacy" }, { slug: "support" }],
        },
      ],
    }),
  ],
});
