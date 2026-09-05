#!/usr/bin/env python3
"""Generate every platform's app icon from images/midi-sink.jpg.

Run from the repo root:  python3 tools/gen_icons.py

Chosen over Android Studio's Asset Studio because the same source art has to
feed four platforms and stay regenerable in-tree (Asset Studio is Android-only
and GUI-driven). Outputs:

  android/app/src/main/res/mipmap-*dpi/ic_launcher.png            legacy square
  android/app/src/main/res/mipmap-*dpi/ic_launcher_foreground.png adaptive fg
  android/app/src/main/res/mipmap-anydpi-v26/ic_launcher*.xml     adaptive defs
  android/app/src/main/res/values/ic_launcher_colors.xml          bg color
  ios/Resources/Assets.xcassets/AppIcon.appiconset/               1024 + json
  packaging/linux/icons/hicolor/*/apps/midi-sink.png              XDG icon theme
  desktop/src/app_icon.h                                          GLFW RGBA blob
  desktop/midi-sink.ico                                           Win32 exe icon
  packaging/macos/midi-sink.icns                                  macOS bundle icon

The art is a full-bleed square: a circular suminagashi motif on washi cream,
with ink touching the edges. Two consequences drive the choices below:

* Android adaptive icons are masked to the central safe zone, so the art
  cannot be used full-bleed as a layer — the mask would crop the motif. The
  foreground instead keys the cream to ALPHA (the art is cleanly bimodal:
  ink L~72, cream L~236) and scales the ink into the safe zone, over a
  background layer painted the same cream. Because the background matches
  the art's own field colour, the result reproduces the original artwork with
  no square seam, and any mask crop only ever eats cream. The same keyed
  layer doubles as the Android 13+ <monochrome> themed icon, since its alpha
  is exactly the ink silhouette.
* iOS forbids alpha in app icons, so that platform gets the opaque
  full-bleed art (iOS's own squircle mask trims only the corners, which are
  cream).
"""
import io
import json
import os
import sys

try:
    import numpy as np
    from PIL import Image, ImageDraw
except ImportError:
    sys.exit("PIL/Pillow and numpy required:  pip install pillow numpy")

SRC = "images/midi-sink.jpg"

# Sampled from the source (see the module docstring): the washi field colour
# and the alpha-key luminance ramp separating cream from ink.
CREAM_HEX = "#F1ECE2"
KEY_CREAM_L = 200.0   # luminance at/above which the art is pure field -> alpha 0
KEY_INK_L = 110.0     # luminance at/below which the art is pure ink   -> alpha 1

# Fraction of the 108dp adaptive canvas the art square occupies. The documented
# always-visible safe zone is 72/108 = 0.667; 0.74 is a mild compromise so the
# motif does not read small under generous launcher masks, and the feather below
# means what spills past the safe zone is already faint.
FG_ART_FRACTION = 0.74

# Radial feather applied to the adaptive foreground's alpha, in units of the
# art square's inscribed radius (1.0 = half the art's side). The source has no
# margin — ink strokes run into its frame — so insetting it raw leaves those
# strokes cut off as hard straight lines against the background (measured: a
# 67/255 step at the boundary). Fading the outermost fringe to nothing dissolves
# the frame instead, which also means a launcher mask never hard-clips ink.
FG_FEATHER_INNER = 0.80
FG_FEATHER_OUTER = 1.00

# Android density buckets: (directory suffix, legacy px, adaptive-layer px).
# Adaptive layers are 108dp squares; legacy launcher icons are 48dp.
DENSITIES = [
    ("mdpi",    48,  108),
    ("hdpi",    72,  162),
    ("xhdpi",   96,  216),
    ("xxhdpi",  144, 324),
    ("xxxhdpi", 192, 432),
]

# Corner rounding for the desktop icons (fraction of the icon's side). The
# artwork is a full-bleed square and stays one — only the corners are softened,
# which is what every desktop shell's icon set does; a hard 90-degree corner
# reads harsh next to them. iOS and Android are NOT rounded here: both mask the
# icon themselves (and iOS forbids alpha outright).
CORNER_RADIUS_FRACTION = 0.18

# Window-icon sizes handed to glfwSetWindowIcon (it picks per use-site).
DESKTOP_SIZES = [32, 48, 64]


def hex_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def load_source():
    im = Image.open(SRC).convert("RGB")
    if im.width != im.height:
        print(f"  warning: source is {im.width}x{im.height}, not square")
    return im


def keyed_ink(im):
    """The art with its cream field keyed to alpha (ink stays opaque)."""
    rgb = im.convert("RGB")
    # Luminance -> alpha via a linear ramp between the two sampled modes.
    lum = rgb.convert("L")
    scale = 255.0 / (KEY_CREAM_L - KEY_INK_L)
    # point(): alpha = clamp((KEY_CREAM_L - L) * scale, 0, 255)
    alpha = lum.point(
        lambda v: max(0, min(255, int((KEY_CREAM_L - v) * scale + 0.5))))
    out = rgb.copy()
    out.putalpha(alpha)
    return out


def resize(im, size):
    return im.resize((size, size), Image.LANCZOS)


def rounded_square(im, size):
    """The artwork at `size`, square, with anti-aliased rounded corners. The
    mask is drawn 4x oversampled and downscaled, because ImageDraw's rounded
    rectangle is hard-edged and a 1:1 mask would alias the curves."""
    art = resize(im.convert("RGB"), size).convert("RGBA")
    ss = 4
    mask = Image.new("L", (size * ss, size * ss), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, size * ss - 1, size * ss - 1),
        radius=int(round(size * ss * CORNER_RADIUS_FRACTION)), fill=255)
    art.putalpha(mask.resize((size, size), Image.LANCZOS))
    return art


def radial_falloff(n):
    """Smoothstep falloff over an n×n grid, 1.0 inside FG_FEATHER_INNER and
    0.0 at FG_FEATHER_OUTER, in inscribed-radius units."""
    axis = (np.arange(n) + 0.5) / n * 2.0 - 1.0          # -1..1 across the art
    r = np.hypot(*np.meshgrid(axis, axis, indexing="xy"))
    t = np.clip((r - FG_FEATHER_INNER) / (FG_FEATHER_OUTER - FG_FEATHER_INNER),
                0.0, 1.0)
    return 1.0 - (t * t * (3.0 - 2.0 * t))               # smoothstep


def feather_alpha(im):
    """Multiply an RGBA image's alpha by a smooth radial falloff (see the
    FG_FEATHER_* constants): opaque within FG_FEATHER_INNER, zero at
    FG_FEATHER_OUTER, measured in inscribed-radius units."""
    rgba = np.asarray(im, dtype=np.float64).copy()
    rgba[:, :, 3] *= radial_falloff(im.width)
    return Image.fromarray(rgba.round().astype(np.uint8))


def write_png(im, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    im.save(path, "PNG", optimize=True)
    print(f"  {path}  ({im.width}x{im.height}, {im.mode})")


def gen_android(src):
    res = "android/app/src/main/res"
    ink = feather_alpha(keyed_ink(src))

    for suffix, legacy_px, layer_px in DENSITIES:
        # Legacy square icon (pre-adaptive launchers / fallback): full-bleed.
        write_png(resize(src.convert("RGB"), legacy_px),
                  f"{res}/mipmap-{suffix}/ic_launcher.png")

        # Adaptive foreground: transparent canvas, ink scaled into the safe
        # zone. Downscale the keyed art in premultiplied space so the cream
        # RGB cannot bleed a light halo into the ink edges.
        art_px = max(1, int(round(layer_px * FG_ART_FRACTION)))
        art = resize(ink.convert("RGBa"), art_px).convert("RGBA")
        canvas = Image.new("RGBA", (layer_px, layer_px), (0, 0, 0, 0))
        off = (layer_px - art_px) // 2
        canvas.paste(art, (off, off))
        write_png(canvas, f"{res}/mipmap-{suffix}/ic_launcher_foreground.png")

    # Background layer is a flat colour (the art's own washi field).
    os.makedirs(f"{res}/values", exist_ok=True)
    with open(f"{res}/values/ic_launcher_colors.xml", "w") as f:
        f.write('<?xml version="1.0" encoding="utf-8"?>\n'
                "<!-- Generated by tools/gen_icons.py — the washi field colour\n"
                "     sampled from images/midi-sink.jpg; the adaptive foreground\n"
                "     is keyed against exactly this value. -->\n"
                "<resources>\n"
                f'    <color name="ic_launcher_background">{CREAM_HEX}</color>\n'
                "</resources>\n")
    print(f"  {res}/values/ic_launcher_colors.xml")

    # Adaptive icon definitions. <monochrome> (API 33+) reuses the keyed
    # foreground: its alpha already is the ink silhouette.
    adaptive = (
        '<?xml version="1.0" encoding="utf-8"?>\n'
        "<!-- Generated by tools/gen_icons.py -->\n"
        '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
        '    <background android:drawable="@color/ic_launcher_background" />\n'
        '    <foreground android:drawable="@mipmap/ic_launcher_foreground" />\n'
        '    <monochrome android:drawable="@mipmap/ic_launcher_foreground" />\n'
        "</adaptive-icon>\n")
    os.makedirs(f"{res}/mipmap-anydpi-v26", exist_ok=True)
    for name in ("ic_launcher.xml", "ic_launcher_round.xml"):
        with open(f"{res}/mipmap-anydpi-v26/{name}", "w") as f:
            f.write(adaptive)
        print(f"  {res}/mipmap-anydpi-v26/{name}")


def gen_ios(src):
    """One 1024 universal icon — what Xcode 14+ single-size app icons want.
    Flattened onto the cream field: iOS rejects icons with an alpha channel."""
    d = "ios/Resources/Assets.xcassets/AppIcon.appiconset"
    icon = resize(src.convert("RGB"), 1024)
    write_png(icon, f"{d}/icon-1024.png")

    with open(f"{d}/Contents.json", "w") as f:
        json.dump({
            "images": [{
                "filename": "icon-1024.png",
                "idiom": "universal",
                "platform": "ios",
                "size": "1024x1024",
            }],
            "info": {"author": "tools/gen_icons.py", "version": 1},
        }, f, indent=2)
        f.write("\n")
    print(f"  {d}/Contents.json")

    root = "ios/Resources/Assets.xcassets/Contents.json"
    with open(root, "w") as f:
        json.dump({"info": {"author": "tools/gen_icons.py", "version": 1}},
                  f, indent=2)
        f.write("\n")
    print(f"  {root}")


def gen_linux_theme(src):
    """XDG icon-theme PNGs. A Wayland compositor never takes an icon from the
    client: it matches the toplevel's app_id to a .desktop file and loads that
    file's Icon= name from the theme, so this (plus packaging/linux/
    midi-sink.desktop and the app_id GLFW sets) is the only thing that gives
    the app an icon in the dock, the app grid and alt-tab.

    Square with rounded corners: GNOME does not mask icons, so what is drawn
    here is exactly what the dock shows."""
    for size in (16, 24, 32, 48, 64, 128, 256):
        write_png(rounded_square(src, size),
                  f"packaging/linux/icons/hicolor/{size}x{size}/apps/midi-sink.png")


def gen_desktop(src):
    """A C header with RGBA pixels for glfwSetWindowIcon — compiled in, so the
    harness needs no runtime asset path."""
    path = "desktop/src/app_icon.h"
    lines = [
        "// app_icon.h — GENERATED by tools/gen_icons.py from images/midi-sink.jpg.",
        "// Do not edit by hand; re-run the generator instead.",
        "//",
        "// RGBA8 pixels for glfwSetWindowIcon (GLFWimage), compiled into the",
        "// harness so it depends on no runtime asset path. The square artwork",
        "// with rounded corners; the washi field is opaque, which keeps the",
        "// mark legible on light and dark shells alike.",
        "// Supported on X11 and Windows; GLFW ignores window icons on Wayland",
        "// and macOS, which take the icon from the .desktop file (see",
        "// packaging/linux/) and the app bundle respectively.",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
    ]
    for size in DESKTOP_SIZES:
        im = rounded_square(src, size)           # GLFWimage wants RGBA
        data = im.tobytes()
        lines.append(f"static const int   sumi_icon_{size}_size = {size};")
        lines.append(f"static const uint8_t sumi_icon_{size}[{len(data)}] = {{")
        for i in range(0, len(data), 24):
            chunk = ", ".join(str(b) for b in data[i:i + 24])
            lines.append(f"    {chunk},")
        lines.append("};")
        lines.append("")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write("\n".join(lines))
    total = sum(size * size * 4 for size in DESKTOP_SIZES)
    print(f"  {path}  ({', '.join(str(s) for s in DESKTOP_SIZES)} px, {total} bytes of pixels)")

    # Windows: glfwSetWindowIcon covers the window and taskbar at runtime, but
    # the icon Explorer shows for midi-sink.exe comes from a linked resource —
    # desktop/midi-sink.rc references this .ico (WIN32-only in CMake).
    ico = "desktop/midi-sink.ico"
    sizes = [(s, s) for s in (16, 24, 32, 48, 64, 128, 256)]
    rounded_square(src, 256).save(ico, "ICO", sizes=sizes)
    print(f"  {ico}  ({', '.join(str(s[0]) for s in sizes)} px)")


def gen_macos(src):
    """macOS app-bundle icon (Phase 5, DECISIONS_4 #6): the harness is a real
    .app now, so the Dock reads CFBundleIconFile from the bundle. Pillow's
    ICNS writer emits every standard slot (16..1024, @1x/@2x) from one
    1024 px master — no iconutil dependency, so the generator stays
    cross-platform. The former runtime Dock-tile blob (app_icon_macos.h) is
    gone with the bare executable it served."""
    path = "packaging/macos/midi-sink.icns"
    os.makedirs(os.path.dirname(path), exist_ok=True)
    rounded_square(src, 1024).save(path, "ICNS")
    print(f"  {path}  (ICNS, 16..1024 px from a 1024 px master)")


def main():
    if not os.path.exists(SRC):
        sys.exit(f"{SRC} not found — run from the repo root")
    # --only <target>[,<target>]: regenerate one platform's assets without
    # touching the others' (Phase 5 working rule: one platform per step).
    targets = {"android": gen_android, "ios": gen_ios, "linux": gen_linux_theme,
               "desktop": gen_desktop, "macos": gen_macos}
    only = None
    args = sys.argv[1:]
    if len(args) >= 2 and args[0] == "--only":
        only = [t.strip() for t in args[1].split(",")]
        unknown = [t for t in only if t not in targets]
        if unknown:
            sys.exit(f"unknown --only target(s) {unknown}; choose from {sorted(targets)}")
    elif args:
        sys.exit("usage: gen_icons.py [--only android,ios,linux,desktop,macos]")
    src = load_source()
    print(f"source: {SRC} ({src.width}x{src.height})")
    for name, fn in targets.items():
        if only and name not in only:
            continue
        print(f"{name}:")
        fn(src)
    print("done.")


if __name__ == "__main__":
    main()
