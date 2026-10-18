@echo off
setlocal
echo =================================================================
echo   Building Yoshi's Island (SNESRecomp Native AOT)
echo =================================================================
rem Check if cmake is already available in PATH; otherwise fallback to local paths
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    if exist "C:\MYAPPLY\mingw64\bin" (
        set "PATH=%PATH%;C:\MYAPPLY\Microsoft Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\MYAPPLY\Microsoft Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;C:\MYAPPLY\mingw64\bin"
    )
)

if not exist build mkdir build
cd build
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

ninja -j 2 yoshis_island_recomp
if %ERRORLEVEL% neq 0 (
    echo Compilation failed!
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

cd ..
echo.
echo =================================================================
echo   Build Successful! 
echo   Executable: build\YoshisIslandRecomp.exe
echo =================================================================
