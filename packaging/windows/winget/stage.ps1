# Fills the staged Vibetuned.MidiSink manifests from a PUBLISHED release tag
# (the asset URL must be public — publish the draft first, pre-release is
# fine for testing). Usage, from the repo root:
#
#   powershell packaging\windows\winget\stage.ps1 v0.5.0-rc.5
#   winget install --manifest packaging\windows\winget\Vibetuned.MidiSink   # the RC DONE check
#
# The first REAL submission to microsoft/winget-pkgs stays a human act
# (wingetcreate new — see ..\..\..\.github\workflows\publish-winget.yml);
# later versions are bumped automatically by that workflow.
param([Parameter(Mandatory = $true)][string]$Tag)
$ErrorActionPreference = "Stop"
# The manifests live in their own subdirectory: winget validate/install parse
# EVERY file in the target directory, so README.md cannot sit beside them.
$dir = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "Vibetuned.MidiSink"
$version = $Tag.TrimStart("v")
$asset = "midi-sink-$version-windows-x64-setup.exe"
$url = "https://github.com/vibetuned/midi-sink/releases/download/$Tag/$asset"

Write-Host "downloading $url"
$tmp = Join-Path $env:TEMP $asset
Invoke-WebRequest $url -OutFile $tmp
$sha = (Get-FileHash $tmp -Algorithm SHA256).Hash.ToUpper()
Remove-Item $tmp

foreach ($f in Get-ChildItem $dir -Filter "*.yaml") {
    $t = Get-Content $f.FullName -Raw
    $t = $t -replace "PackageVersion: .*", "PackageVersion: $version"
    $t = $t -replace "InstallerUrl: .*", "InstallerUrl: $url"
    $t = $t -replace "InstallerSha256: .*", "InstallerSha256: $sha"
    Set-Content $f.FullName $t -Encoding utf8 -NoNewline
}
Write-Host "manifests staged at $version (sha256 $sha)"
Write-Host "test with: winget install --manifest $dir"
