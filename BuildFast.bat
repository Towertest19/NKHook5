@echo off
echo === NKHook5 Fast Build ===
echo.

:: Only delete build outputs, NOT the cached dependencies
powershell -Command "if (Test-Path bin\NKHook5\Release\NKHook5.dll) { Remove-Item bin\NKHook5\Release\NKHook5.dll -Force -ErrorAction SilentlyContinue }"
powershell -Command "if (Test-Path bin\Loader\Release\wininet.dll) { Remove-Item bin\Loader\Release\wininet.dll -Force -ErrorAction SilentlyContinue }"
powershell -Command "if (Test-Path bin\NKHook5\Debug\NKHook5.dll) { Remove-Item bin\NKHook5\Debug\NKHook5.dll -Force -ErrorAction SilentlyContinue }"
powershell -Command "if (Test-Path bin\Loader\Debug\wininet.dll) { Remove-Item bin\Loader\Debug\wininet.dll -Force -ErrorAction SilentlyContinue }"

if not exist bin mkdir bin
cd bin

:: Create Artifacts directories
if not exist Artifacts mkdir Artifacts
if not exist Artifacts\Debug mkdir Artifacts\Debug
if not exist Artifacts\Release mkdir Artifacts\Release

:: Configure (dependencies cached after first build)
del CMakeCache.txt 2>nul
cmake .. -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 17 2022" -A Win32

:: Build both configurations
cmake --build . --config Debug --target NKHook5 --target wininet
cmake --build . --config Release --target NKHook5 --target wininet

:: Copy to Artifacts
copy "NKHook5\Debug\NKHook5.dll" "Artifacts\Debug\NKHook5.dll" 2>nul
copy "Loader\Debug\wininet.dll" "Artifacts\Debug\wininet.dll" 2>nul
copy "NKHook5\Release\NKHook5.dll" "Artifacts\Release\NKHook5.dll" 2>nul
copy "Loader\Release\wininet.dll" "Artifacts\Release\wininet.dll" 2>nul

echo.
echo === Build Complete ===
timeout 3 > NUL
