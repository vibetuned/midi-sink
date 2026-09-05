/**
 * Stop a running `npm run preview`. Astro 7's preview is a DAEMON: `astro
 * preview` returns after starting a background server (it says "already
 * running" on the next call), so killing the terminal does not stop it — the
 * supported way is `astro preview stop`, which this wraps. `--clean` also
 * removes the composed dist/marble/ copy so dist/ is exactly what `astro
 * build` produced.
 *
 *   npm run preview:stop
 *   npm run preview:clean
 *   npx astro preview status | logs     (the daemon's own commands)
 */
import { spawnSync } from "node:child_process";
import { existsSync, rmSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..");
const marble = join(root, "dist", "marble");
const astro = join(root, "node_modules", ".bin", process.platform === "win32" ? "astro.cmd" : "astro");

const r = spawnSync(astro, ["preview", "stop"], { cwd: root, encoding: "utf8", shell: process.platform === "win32" });
// astro prints one JSON object per line ({message, level}); show the message.
const lines = ((r.stdout || "") + (r.stderr || "")).trim().split("\n").filter(Boolean)
  .map((l) => { try { return JSON.parse(l).message ?? l; } catch { return l; } });
console.log(lines.length ? lines.map((l) => `preview: ${l.trim()}`).join("\n") : `preview: astro preview stop exited ${r.status}`);

// Belt and braces for a server started by an older astro (a plain foreground
// process): kill any leftover `astro preview` command line.
if (process.platform !== "win32") spawnSync("pkill", ["-f", "astro preview --port"]);

if (process.argv.includes("--clean") && existsSync(marble)) {
  rmSync(marble, { recursive: true, force: true });
  console.log("preview: removed dist/marble/ (the composed wasm copy)");
}
