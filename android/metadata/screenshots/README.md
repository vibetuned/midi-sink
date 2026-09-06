# Screenshot plan — Google Play (author input from real sessions)

Play wants, per form factor, 2–8 screenshots, JPEG or 24-bit PNG, each side
320–3840 px, aspect between 16:9 and 9:16 (Play Console rejects anything
squarer). Provide the **7-inch and 10-inch tablet** sets (the app targets
tablets) and a **phone** set (the listing shows it to phones too):

* **10-inch tablet** — the Galaxy Tab S8 Ultra is 2960×1848 (16:10): use its
  native screenshots (`adb exec-out screencap -p > shot.png`).
* **7-inch tablet** — same captures are accepted (the ratio is what matters).
* **Phone** — crop the 16:10 captures to 16:9 (2960×1665) or capture on a
  phone; both modes work on any Android 10+ device with OpenGL ES 3.

Six slots, in this order (the first two are what the listing shows first):

1. **Marble mode, mid-performance** — a ROLI or Osmose playing: rings, combs
   and a vortex on the paper. The hero shot.
2. **Play mode, chromatic grid** — several fingers down, joystick rings and
   thumbs visible, the floating strip top-left, drops under the fingers.
3. **S-Pen legato** — a glissando trail of drops across the piano grid, the
   pen's hover ghost visible.
4. **Piano grid** — the two-row keyboard lattice with the black-key corridor,
   one hand playing.
5. **Settings sheet** — Mode, Note bend, Slide, Outbound MIDI with the USB
   status line reading *active*, About showing the tag.
6. **A finished sheet** — after a paper dip, the print alone.

Also needed: the **feature graphic** (1024×500 PNG/JPEG, no alpha) — a crop
of a real print with the wordmark; and the **app icon** (512×512 PNG) from
`tools/gen_icons.py` output (`packaging/` / the launcher mipmaps' source).

Capture from a tagged build so About in slot 5 reads the release. Do not
retouch the marbling; do crop the status bar if it carries personal
information (time and battery are fine).
