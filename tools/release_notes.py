#!/usr/bin/env python3
"""CHANGELOG.md -> release notes for one version (Phase 5 §3, DECISIONS_4 #12).

  release_notes.py docs/CHANGELOG.md --version 1.0.0 --out notes.md
                   [--commit <sha>] [--strict]

The condensed-evidence practice continues: the changelog IS the release
notes, no third format. The notes are the `## v<version>` section of the
changelog, verbatim, under a title line. When the section does not exist yet
(a dry-run on a test tag, or a real tag cut before the changelog was
updated) the LATEST section is used and the draft is marked as such; with
--strict that case is an error, which is how a real tag is blocked until the
changelog carries its section.

Exit codes: 0 ok, 1 --strict and no section, 2 usage / file error.
"""
import argparse
import re
import sys


def sections(text):
    """[(heading_line, version_or_None, body)] for every '## ' heading."""
    out = []
    parts = re.split(r"^(## .*)$", text, flags=re.M)
    # parts: [preamble, heading, body, heading, body, ...]
    for i in range(1, len(parts) - 1, 2):
        heading = parts[i].strip()
        body = parts[i + 1].strip("\n")
        m = re.match(r"## v(\d+\.\d+\.\d+)", heading)
        out.append((heading, m.group(1) if m else None, body))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("changelog")
    ap.add_argument("--version", required=True, help="X.Y.Z (no leading v)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--commit", default="")
    ap.add_argument("--strict", action="store_true",
                    help="fail when the changelog has no section for --version")
    a = ap.parse_args()

    try:
        text = open(a.changelog, encoding="utf-8").read()
    except OSError as e:
        print(f"cannot read {a.changelog}: {e}", file=sys.stderr)
        return 2
    secs = sections(text)
    if not secs:
        print("no '## ' sections in the changelog", file=sys.stderr)
        return 2

    base = re.match(r"\d+\.\d+\.\d+", a.version)
    want = base.group(0) if base else a.version
    hit = next((s for s in secs if s[1] == want), None)
    draft = hit is None
    if draft:
        if a.strict:
            print(f"::error::CHANGELOG.md has no '## v{want}' section — add it before tagging",
                  file=sys.stderr)
            return 1
        hit = secs[0]
        print(f"::warning::CHANGELOG.md has no section for v{want}; drafting from "
              f"'{hit[0]}'", file=sys.stderr)

    heading, _, body = hit
    lines = [f"# midi-sink v{a.version}", ""]
    if draft:
        lines += [f"> **DRAFT** — `CHANGELOG.md` has no section for v{want} yet; these are "
                  f"the latest notes (`{heading[3:]}`). Add the section before publishing.",
                  ""]
    else:
        subtitle = heading[3:]
        if "—" in subtitle:
            lines += [f"_{subtitle.split('—', 1)[1].strip()}_", ""]
    lines += [body, ""]
    lines += ["---",
              "Notes generated from `docs/CHANGELOG.md` by the release workflow"
              + (f" at commit `{a.commit[:12]}`" if a.commit else "") + "."]
    with open(a.out, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {a.out} ({'draft from latest section' if draft else 'section v' + want})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
