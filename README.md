# midi-sink

A suminagashi (Japanese ink-marbling) visualizer played by expressive MIDI —
and, on the tablets, an MPE instrument of its own. Every note is a drop of ink
on water; pressure feeds it, bends comb it, the Airwave's hands stir it, a
stylus threads it. The engine (`libsumi`, a C-ABI core on sokol_gfx) is the
same bytes on macOS (Metal), Windows (D3D11), Linux (OpenGL), iPad (Metal),
Android (GLES3) and the browser (WebGPU).

**Documentation:** <https://midi-sink.vibetuned.com/> — the
[user guide](https://midi-sink.vibetuned.com/guide/install/), the
[operator book](https://midi-sink.vibetuned.com/operators/) with live demos of
every deformation, the [MIDI implementation chart](https://midi-sink.vibetuned.com/reference/midi-chart/),
the [settings reference](https://midi-sink.vibetuned.com/reference/settings/),
and the [design notes](https://midi-sink.vibetuned.com/notes/changelog/).

**Performances:** <https://midi-sink.vibetuned.com/gallery/> — Everything In
Its Right Place on a ROLI Piano and Airwave, Autumn Leaves on a Travel Sax,
La Guaracha and Canon in D on the iPad, and the Jaffer tribute, *Ali Paşa*.

**Try it in the browser:** <https://midi-sink.vibetuned.com/marble/> (Marble
mode; Web MIDI on Chrome and Edge).

## Install

| Platform | How |
|---|---|
| macOS | `brew install --cask vibetuned/tap/midi-sink`, or the DMG from [Releases](https://github.com/vibetuned/midi-sink/releases) |
| Windows | `winget install Vibetuned.MidiSink`, or the installer / portable zip from Releases |
| Linux | the apt repository at `https://midi-sink.vibetuned.com/apt` (`stable` and `rc` suites), or the `.deb` / tarball from Releases |
| iPad, Android | TestFlight and Google Play — see the [install page](https://midi-sink.vibetuned.com/guide/install/) |

Plug in a MIDI instrument and it appears in the settings window; every
setting is explained in the [settings reference](https://midi-sink.vibetuned.com/reference/settings/).

## Repository

| Path | What |
|---|---|
| `core/` | `libsumi`: the marbling engine behind `sumi_core.h` |
| `hostmpe/` | the tablets' shared host library: voice allocation, joysticks, transports |
| `desktop/`, `ios/`, `android/`, `web/` | the shells |
| `site/` | the documentation site (Astro Starlight) |
| `packaging/`, `.github/workflows/` | the release lanes and channel workflows |
| `docs/` | [BUILD.md](docs/BUILD.md) (build, run, test, package, every platform), [PROJECT_SPEC.md](docs/PROJECT_SPEC.md), [DECISIONS.md](docs/DECISIONS.md), [CHANGELOG.md](docs/CHANGELOG.md), [ROADMAP.md](docs/ROADMAP.md) |
| `tools/`, `tests/` | the gates, analysers and headless suites ([tools/README.md](tools/README.md)) |

Building from source, the lab bench behind `--dev`, the release spine and the
per-platform lanes: **[docs/BUILD.md](docs/BUILD.md)**.

## License

midi-sink is free software, licensed under the GNU Affero General Public
License v3.0 — see [LICENSE](LICENSE).

Builds distributed through an application store carry one additional
permission under section 7 of that licence, so that the store's signing and
device terms do not conflict with the AGPL — see
[LICENSE-APPSTORE-EXCEPTION.md](LICENSE-APPSTORE-EXCEPTION.md). It applies
only to store copies; the source, and anything you build from it, stay plain
AGPL-3.0.
