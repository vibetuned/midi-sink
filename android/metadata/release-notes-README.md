# Release notes — how they are written here

Play Console → *Release* → **Release notes**, language **en-US**.

**Hard limit: 500 Unicode characters per language.** Play truncates silently
past that, so count before pasting:

```sh
python3 -c "import sys;t=open(sys.argv[1]).read().rstrip('\n');print(len(t),'/ 500')" \
  android/metadata/release-notes-1.0.0-en-US.txt
```

`docs/CHANGELOG.md` stays the engineering record — decision numbers, ABI
versions, per-step detail — and is *not* pasteable. The store note is that
section condensed to what a player would notice, which for `v1.0.0` means an
introduction rather than a change list: it is the first public release, so
there is nothing to contrast it with.

**iOS has no equivalent for a first release.** App Store Connect's *What's New
in This Version* is "not available for the first version of the app" — it
appears only from the second version on. For the `1.0.0` TestFlight build the
field to fill is **Test Information → What to Test**, and `ios/metadata/listing.md`
already carries that text. `release-notes-1.0.1-en-US.md` here holds a longer
iOS-worded draft to adapt when the first update ships (ASC allows 4000
characters, so it need not be as terse as Play's).

| File | Store | Field | Limit |
|---|---|---|---|
| `release-notes-1.0.0-en-US.txt` | Play | Release notes (en-US) | 500 |
| `release-notes-1.0.1-en-US.md` | both | Play notes + ASC What's New | 500 / 4000 |
