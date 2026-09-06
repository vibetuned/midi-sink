# winget conventions — Vibetuned.MidiSink

The in-tree record of the winget manifest (ROADMAP_4 Step 29, DECISIONS_4
Step 29 entries). `microsoft/winget-pkgs` is the source of truth once the
package exists there; these files keep the conventions reviewable and give
release candidates an installable manifest without touching winget-pkgs.

* **Identity:** `Vibetuned.MidiSink`, publisher `Vibetuned`, package name
  `midi-sink`, license `AGPL-3.0-only` (DECISIONS_4 #1).
* **URLs:** the frozen Step-26 pages (DECISIONS_4 #22) — homepage
  `https://midi-sink.vibetuned.com/`, support `…/support/`, privacy
  `…/privacy/`.
* **Installer:** the Inno Setup per-user x64 setup from the release lane
  (`midi-sink-<version>-windows-x64-setup.exe`, `Scope: user`, no UAC).

## First submission (human, one-time)

Moderation wants a human on a package's initial PR. After the first REAL
(non-pre-release) release is published:

```pwsh
wingetcreate new https://github.com/vibetuned/midi-sink/releases/download/v<V>/midi-sink-<V>-windows-x64-setup.exe
# identifier Vibetuned.MidiSink, values as in these files — then --submit
```

Until that PR merges, `publish-winget.yml` detects the missing manifest and
skips cleanly.

## Testing a release candidate

Pre-releases never go to winget-pkgs (the workflow skips them). The RC DONE
check installs from HERE instead:

```pwsh
powershell packaging\windows\winget\stage.ps1 v0.5.0-rc.5   # fills version + sha256 from the published RC
winget install --manifest packaging\windows\winget\Vibetuned.MidiSink
winget uninstall Vibetuned.MidiSink
git checkout -- packaging/windows/winget                    # drop the staged values
```

(The manifests sit in the `Vibetuned.MidiSink/` subdirectory because winget
parses every file in the directory it is pointed at — this README beside them
breaks `winget validate`.)

(`winget settings` must have `LocalManifestFiles` enabled once:
`winget settings --enable LocalManifestFiles`, an admin prompt.)
