@echo off
setlocal
title Super Mario World 2: Yoshi's Island (SNESRecomp Native AOT)
echo =================================================================
echo   Super Mario World 2: Yoshi's Island - Native recomp-ui Host
echo =================================================================
echo Controls:
echo   W / A / S / D or Arrows : D-Pad (Walk, Aim Up, Crouch)
echo   J or Space              : Jump / Flutter Jump (SNES B)
echo   K or Z                  : Eat / Tongue / Spit (SNES Y)
echo   L or X                  : Throw Egg / Aim (SNES A)
echo   I or C                  : Lock Aim Cursor (SNES X)
echo   Q / E                   : L / R Shoulder
echo   Enter                   : START (Pause / Confirm)
echo   Tab                     : SELECT (Item Menu)
echo   F11                     : Toggle Fullscreen
echo   Esc                     : Quit
echo Gamepads:
echo   Direct plug-and-play support for Xbox / PlayStation controllers!
echo =================================================================
echo.

set "EXE_PATH="
if exist "build\YoshisIslandRecomp.exe" set "EXE_PATH=build\YoshisIslandRecomp.exe"
if not defined EXE_PATH if exist "build\yoshis_island_recomp.exe" set "EXE_PATH=build\yoshis_island_recomp.exe"
if not defined EXE_PATH if exist "YoshisIslandRecomp.exe" set "EXE_PATH=YoshisIslandRecomp.exe"
if not defined EXE_PATH if exist "yoshis_island_recomp.exe" set "EXE_PATH=yoshis_island_recomp.exe"

if not defined EXE_PATH (
    echo Executable not found. Running build.bat...
    call build.bat
    if exist "build\YoshisIslandRecomp.exe" set "EXE_PATH=build\YoshisIslandRecomp.exe"
)

if defined EXE_PATH (
    start "" "%EXE_PATH%" %*
) else (
    echo Error: Could not locate YoshisIslandRecomp.exe!
    pause
)
