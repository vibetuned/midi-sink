@echo off
rem Windows build convenience wrapper: applies the MSVC x64 environment, then
rem forwards its arguments, e.g.
rem   build_win.bat cmake -B build -G Ninja
rem   build_win.bat cmake --build build
rem   build_win.bat ctest --test-dir build
rem Requires VS 2022 (any edition) with the C++ toolset, CMake >= 3.24 and
rem Ninja (both findable on PATH or in their default install locations).
setlocal enabledelayedexpansion

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo build_win: vswhere.exe not found - is Visual Studio installed? 1>&2
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo build_win: no VS installation with the C++ toolset found 1>&2
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul

rem Common install locations, appended so an existing PATH entry wins.
if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "PATH=%PATH%;%ProgramFiles%\CMake\bin"
if exist "%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe\ninja.exe" set "PATH=%PATH%;%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe"

%*
endlocal
