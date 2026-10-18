param(
    [string]$BuildType = "Release",
    [string]$Target = "yoshis_island_recomp",
    [int]$Jobs = 2
)

$ErrorActionPreference = "Stop"

Write-Host "=== Super Mario World 2: Yoshi's Island (SNESRecomp Native AOT) Builder ===" -ForegroundColor Cyan

# Prioritize CMake and Ninja from PATH; fallback to local paths if missing
if (-not (Get-Command cmake -ErrorAction SilentlyContinue) -or -not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    if (Test-Path "C:\MYAPPLY\mingw64\bin") {
        $env:PATH = "$env:PATH;C:\MYAPPLY\Microsoft Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\MYAPPLY\Microsoft Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;C:\MYAPPLY\mingw64\bin"
    }
}

if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Push-Location "build"
try {
    cmake .. -G "Ninja" "-DCMAKE_BUILD_TYPE=$BuildType"
    ninja -j $Jobs $Target
    Write-Host "Build successful! Target: $Target in build/" -ForegroundColor Green
}
finally {
    Pop-Location
}
