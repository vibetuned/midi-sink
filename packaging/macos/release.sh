#!/usr/bin/env bash
# macOS release packaging (Phase 5 §3, ROADMAP_4 Step 27, DECISIONS_4 #31–#34):
# a built midi-sink.app  ->  signed, notarized, stapled, universal DMG.
#
#   packaging/macos/release.sh <path/to/midi-sink.app> <version> <out-dir>
#
# Environment (all optional — absent means "ad-hoc, unsigned, not notarized",
# which is what a local run or a fork produces; a Developer ID makes it real):
#   SIGN_IDENTITY        "Developer ID Application: … (TEAMID)"   default "-" (ad-hoc)
#   NOTARY_APPLE_ID + NOTARY_PASSWORD + NOTARY_TEAM_ID     notarytool with an Apple ID
#                                                          and an app-specific password
#   NOTARY_KEY_P8 + NOTARY_KEY_ID + NOTARY_ISSUER          …or an App Store Connect API
#                                                          key (the .p8 contents)
#   REQUIRE_NOTARIZATION=1   fail instead of skipping when no notary credentials
#
# Order matters (Gatekeeper-clean OFFLINE, not just online): sign the app with
# a secure timestamp -> notarize the app -> STAPLE THE APP -> build the DMG
# around the stapled app -> sign the DMG -> notarize the DMG -> staple the DMG.
# Stapling only the DMG leaves the dragged-out .app without a ticket, so a
# first launch with no network would be refused.
#
# Same script for CI and for a local dry run: `packaging/macos/release.sh
# build/desktop/midi-sink.app 0.0.0-local dist` proves the DMG mechanics on any
# Mac without credentials; the release lane adds the identity and the notary.
set -euo pipefail

app="${1:?app bundle}"; version="${2:?version}"; out="${3:?out dir}"
identity="${SIGN_IDENTITY:--}"
name="midi-sink-${version}-macos-universal"
mkdir -p "$out"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
bin="$app/Contents/MacOS/midi-sink"
log() { printf '\n== %s\n' "$*"; }

log "bundle: $app (version $version)"
[[ -x "$bin" ]] || { echo "::error::no executable at $bin"; exit 1; }
archs="$(lipo -archs "$bin")"; echo "architectures: $archs"
for a in arm64 x86_64; do [[ " $archs " == *" $a "* ]] || { echo "::error::not universal — missing $a"; exit 1; }; done
plist_v="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$app/Contents/Info.plist")"
echo "Info.plist CFBundleShortVersionString: $plist_v"

# ---- notary credentials --------------------------------------------------
notary=()
if [[ -n "${NOTARY_KEY_P8:-}" && -n "${NOTARY_KEY_ID:-}" && -n "${NOTARY_ISSUER:-}" ]]; then
  printf '%s' "$NOTARY_KEY_P8" > "$work/AuthKey.p8"
  notary=(--key "$work/AuthKey.p8" --key-id "$NOTARY_KEY_ID" --issuer "$NOTARY_ISSUER")
  echo "notary: App Store Connect API key $NOTARY_KEY_ID"
elif [[ -n "${NOTARY_APPLE_ID:-}" && -n "${NOTARY_PASSWORD:-}" && -n "${NOTARY_TEAM_ID:-}" ]]; then
  notary=(--apple-id "$NOTARY_APPLE_ID" --password "$NOTARY_PASSWORD" --team-id "$NOTARY_TEAM_ID")
  echo "notary: Apple ID $NOTARY_APPLE_ID, team $NOTARY_TEAM_ID"
elif [[ "${REQUIRE_NOTARIZATION:-0}" == "1" ]]; then
  echo "::error::REQUIRE_NOTARIZATION=1 but no notary credentials in the environment"; exit 1
else
  echo "notary: none — the DMG will NOT be notarized (local / fork build)"
fi
[[ "$identity" != "-" || ${#notary[@]} -eq 0 ]] || { echo "::error::notarization needs a Developer ID identity, not ad-hoc"; exit 1; }

notarize() {   # <file> <label>
  log "notarize $2"
  local json; json="$(xcrun notarytool submit "$1" "${notary[@]}" --wait --output-format json)"
  echo "$json"
  local status; status="$(python3 -c 'import sys,json; print(json.load(sys.stdin).get("status",""))' <<<"$json")"
  if [[ "$status" != "Accepted" ]]; then
    local id; id="$(python3 -c 'import sys,json; print(json.load(sys.stdin).get("id",""))' <<<"$json")"
    echo "::error::notarization of $2 was '$status' (submission $id) — log follows"
    [[ -n "$id" ]] && xcrun notarytool log "$id" "${notary[@]}" || true
    exit 1
  fi
}

# ---- 1. sign the app (timestamped when real) -------------------------------
log "codesign app with '$identity'"
ts=(--timestamp); [[ "$identity" == "-" ]] && ts=(--timestamp=none)
codesign --force --deep --sign "$identity" --options runtime "${ts[@]}" \
  --entitlements "$(dirname "$0")/entitlements.plist" "$app"
codesign --verify --deep --strict --verbose=2 "$app"
codesign -dvv "$app" 2>&1 | grep -E 'Authority|TeamIdentifier|Timestamp|flags' || true

# ---- 2. notarize + staple the app ------------------------------------------
if (( ${#notary[@]} )); then
  ditto -c -k --keepParent "$app" "$work/app.zip"
  notarize "$work/app.zip" "midi-sink.app"
  xcrun stapler staple "$app"
  xcrun stapler validate "$app"
fi

# ---- 3. the DMG ------------------------------------------------------------
log "build $name.dmg"
stage="$work/stage"; mkdir -p "$stage"
ditto "$app" "$stage/midi-sink.app"
ln -s /Applications "$stage/Applications"
dmg="$out/$name.dmg"; rm -f "$dmg"
hdiutil create -volname "midi-sink $version" -srcfolder "$stage" -ov -format UDZO -imagekey zlib-level=9 "$dmg" >/dev/null
codesign --force --sign "$identity" "${ts[@]}" "$dmg"
codesign --verify --verbose=2 "$dmg"

# ---- 4. notarize + staple the DMG ------------------------------------------
if (( ${#notary[@]} )); then
  notarize "$dmg" "$name.dmg"
  xcrun stapler staple "$dmg"
  xcrun stapler validate "$dmg"
  log "Gatekeeper assessment"
  spctl --assess --type open --context context:primary-signature --verbose=2 "$dmg"
  # mount and assess the app as a user would launch it
  mnt="$(hdiutil attach -nobrowse -readonly "$dmg" | awk -F'\t' '/\/Volumes\//{print $NF}')"
  spctl --assess --type execute --verbose=2 "$mnt/midi-sink.app"
  hdiutil detach "$mnt" >/dev/null
fi

# ---- 5. checksums + report -------------------------------------------------
( cd "$out" && shasum -a 256 "$name.dmg" > "$name.dmg.sha256" && cat "$name.dmg.sha256" )
log "done: $dmg ($(du -h "$dmg" | cut -f1)); notarized: $([[ ${#notary[@]} -gt 0 ]] && echo yes || echo NO)"
