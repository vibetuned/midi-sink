@echo off
rem Builds the Windows installer locally from the current tree's binary
rem (the release lane runs the same .iss - see release.yml's windows job).
rem
rem   packaging\windows\build_installer.bat            (version from git describe)
rem   packaging\windows\build_installer.bat 1.0.0      (exact version)
rem
rem Run from the repo root, after a build. Output lands in dist\.
setlocal enabledelayedexpansion

set "ROOT=%~dp0..\.."
pushd "%ROOT%"

if not exist "build\desktop\midi-sink.exe" (
    echo build_installer: build\desktop\midi-sink.exe not found - build first. 1>&2
    popd & exit /b 1
)

rem ISCC: user-scope install first (this box), then machine-wide.
set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo build_installer: ISCC.exe not found - winget install JRSoftware.InnoSetup 1>&2
    popd & exit /b 1
)

set "VERSION=%~1"
if "%VERSION%"=="" (
    for /f "usebackq tokens=*" %%v in (`git describe --tags --always --dirty 2^>nul`) do set "VERSION=%%v"
    if "!VERSION:~0,1!"=="v" set "VERSION=!VERSION:~1!"
)
if "%VERSION%"=="" set "VERSION=0.0.0-local"

if not exist dist mkdir dist
"%ISCC%" /Qp /DAppVersion=%VERSION% /DAppExe="%CD%\build\desktop\midi-sink.exe" ^
    /DOutDir="%CD%\dist" /DOutName=midi-sink-%VERSION%-windows-x64-setup ^
    packaging\windows\midi-sink.iss
if errorlevel 1 ( popd & exit /b 1 )

echo.
echo built: dist\midi-sink-%VERSION%-windows-x64-setup.exe
echo install:   dist\midi-sink-%VERSION%-windows-x64-setup.exe /VERYSILENT /CURRENTUSER
echo uninstall: "%%LOCALAPPDATA%%\Programs\midi-sink\unins000.exe"
popd
endlocal
