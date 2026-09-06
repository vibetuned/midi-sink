; midi-sink Windows installer (Phase 5 §2-3, ROADMAP_4 Step 29).
; Inno Setup 6 — hand-authored and committed (the sibling apps' NSIS scripts
; are Tauri-generated; here the script IS the source). Built by the release
; lane and locally the same way:
;
;   iscc /DAppVersion=1.0.0 /DAppExe=..\..\build\desktop\midi-sink.exe ^
;        /DOutDir=..\..\dist /DOutName=midi-sink-1.0.0-windows-x64-setup ^
;        packaging\windows\midi-sink.iss
;
; Per-user install (no UAC, no admin): {autopf} resolves to
; %LOCALAPPDATA%\Programs under PrivilegesRequired=lowest — the scope winget
; expects for a user-mode package. Registers the Start-menu entry, the icon
; (from the exe's own resource) and the uninstaller. Settings in
; %APPDATA%\midi-sink are the USER'S: uninstall leaves them unless the user
; opts in when asked (the prompt is skipped in silent/winget uninstalls, so
; automation never deletes user data).

#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif
#ifndef AppExe
  #define AppExe "..\..\build\desktop\midi-sink.exe"
#endif
#ifndef OutDir
  #define OutDir "..\..\dist"
#endif
#ifndef OutName
  #define OutName "midi-sink-" + AppVersion + "-windows-x64-setup"
#endif

[Setup]
; Stable AppId so upgrades replace instead of stacking (never change it).
AppId={{7E1FA9E3-58A4-4E41-9F3B-6BAD11C6C5A2}
AppName=midi-sink
AppVersion={#AppVersion}
AppVerName=midi-sink {#AppVersion}
AppPublisher=Vibetuned
AppPublisherURL=https://midi-sink.vibetuned.com/
AppSupportURL=https://midi-sink.vibetuned.com/support/
AppUpdatesURL=https://midi-sink.vibetuned.com/
DefaultDirName={autopf}\midi-sink
DefaultGroupName=midi-sink
DisableProgramGroupPage=yes
DisableDirPage=no
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile=..\..\LICENSE
OutputDir={#OutDir}
OutputBaseFilename={#OutName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\midi-sink.exe
UninstallDisplayName=midi-sink

[Files]
Source: "{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\midi-sink"; Filename: "{app}\midi-sink.exe"
Name: "{autodesktop}\midi-sink"; Filename: "{app}\midi-sink.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Run]
Filename: "{app}\midi-sink.exe"; Description: "Launch midi-sink"; Flags: nowait postinstall skipifsilent

[Code]
// Settings survive uninstall unless the user opts in (Phase 5 §2 / the
// step-29 brief). Interactive uninstalls ask once; silent ones (winget)
// never touch the data.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  dir: string;
begin
  if CurUninstallStep = usPostUninstall then begin
    dir := ExpandConstant('{userappdata}\midi-sink');
    if (not UninstallSilent) and DirExists(dir) then begin
      if MsgBox('Also remove your midi-sink settings and CC map?'#13#10 +
                dir, mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(dir, True, True, True);
    end;
  end;
end;
