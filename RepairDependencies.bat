@echo off
setlocal EnableExtensions EnableDelayedExpansion

pushd "%~dp0" || exit /b 1

echo === NKHook5 dependency repair ===
echo This refreshes the CMake/FetchContent dependency cache, then rebuilds json_spirit.
echo.

where cmake >nul 2>nul
if errorlevel 1 (
	echo ERROR: cmake was not found in PATH.
	echo Install Visual Studio 2022 with C++ CMake tools, or run this from a VS Developer Command Prompt.
	popd
	exit /b 1
)

if not exist "bin" mkdir "bin"

echo [1/5] Removing stale CMake configure files...
if exist "bin\CMakeCache.txt" del /f /q "bin\CMakeCache.txt"
if exist "bin\CMakeFiles" rmdir /s /q "bin\CMakeFiles"

echo [2/5] Removing cached FetchContent sources for Boost/json_spirit...
for %%D in (
	"bin\_deps\boost-build"
	"bin\_deps\boost-src"
	"bin\_deps\boost-subbuild"
	"bin\_deps\jsonspirit-build"
	"bin\_deps\jsonspirit-src"
	"bin\_deps\jsonspirit-subbuild"
) do (
	if exist %%~D (
		echo   deleting %%~D
		rmdir /s /q %%~D
	)
)

echo [3/5] Configuring Win32 Release project...
cmake -S . -B bin -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 17 2022" -A Win32
if errorlevel 1 goto :failed

echo [4/5] Rebuilding json_spirit...
cmake --build bin --config Release --target json_spirit
if errorlevel 1 goto :failed

echo [5/5] Building NKHook5 and loader...
cmake --build bin --config Release --target NKHook5 --target wininet
if errorlevel 1 goto :failed

if not exist "bin\Artifacts\Release" mkdir "bin\Artifacts\Release"
copy /y "bin\NKHook5\Release\NKHook5.dll" "bin\Artifacts\Release\NKHook5.dll" >nul 2>nul
copy /y "bin\Loader\Release\wininet.dll" "bin\Artifacts\Release\wininet.dll" >nul 2>nul

echo.
echo Repair complete.
echo Release artifacts:
echo   bin\Artifacts\Release\NKHook5.dll
echo   bin\Artifacts\Release\wininet.dll
popd
exit /b 0

:failed
echo.
echo Repair failed. Re-run this from a Visual Studio 2022 Developer Command Prompt and check the first error above.
popd
exit /b 1
