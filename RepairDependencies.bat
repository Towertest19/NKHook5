@echo off
setlocal EnableExtensions EnableDelayedExpansion

pushd "%~dp0" || exit /b 1

echo === NKHook5 dependency repair ===
echo This repairs stale json_spirit CMake files, then rebuilds NKHook5.
echo.

where cmake >nul 2>nul
if errorlevel 1 (
	echo ERROR: cmake was not found in PATH.
	echo Install Visual Studio 2022 with C++ CMake tools, or run this from a VS Developer Command Prompt.
	popd
	exit /b 1
)

if not exist "bin" mkdir "bin"

echo [1/5] Removing generated CMake projects and stale configure files...
for %%D in (
	"bin\CMakeFiles"
	"bin\NKHook5"
	"bin\Loader"
	"bin\Common"
	"bin\DevKit"
	"bin\cmake"
	"bin\CMakeScripts"
	"bin\json_spirit.dir"
) do (
	if exist %%~D (
		echo   deleting %%~D
		rmdir /s /q %%~D
	)
)
for %%F in (
	"bin\CMakeCache.txt"
	"bin\cmake_install.cmake"
	"bin\NKHook5.sln"
	"bin\NKHook5.vcxproj"
	"bin\NKHook5.vcxproj.filters"
	"bin\json_spirit.vcxproj"
	"bin\json_spirit.vcxproj.filters"
	"bin\ZERO_CHECK.vcxproj"
	"bin\ZERO_CHECK.vcxproj.filters"
	"bin\ALL_BUILD.vcxproj"
	"bin\ALL_BUILD.vcxproj.filters"
) do (
	if exist %%~F (
		echo   deleting %%~F
		del /f /q %%~F
	)
)

echo [2/5] Removing cached FetchContent folders for json_spirit only...
for %%D in (
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

echo [4/5] json_spirit is header-only for NKHook5; no library rebuild required.

echo [5/5] Building NKHook5 and loader...
cmake --build bin --config Debug --target NKHook5 --target wininet
if errorlevel 1 goto :failed
cmake --build bin --config Release --target NKHook5 --target wininet
if errorlevel 1 goto :failed

if not exist "bin\Artifacts\Debug" mkdir "bin\Artifacts\Debug"
if not exist "bin\Artifacts\Release" mkdir "bin\Artifacts\Release"
copy /y "bin\NKHook5\Debug\NKHook5.dll" "bin\Artifacts\Debug\NKHook5.dll" >nul 2>nul
copy /y "bin\Loader\Debug\wininet.dll" "bin\Artifacts\Debug\wininet.dll" >nul 2>nul
copy /y "bin\NKHook5\Release\NKHook5.dll" "bin\Artifacts\Release\NKHook5.dll" >nul 2>nul
copy /y "bin\Loader\Release\wininet.dll" "bin\Artifacts\Release\wininet.dll" >nul 2>nul

echo.
echo Repair complete.
echo Artifacts:
echo   bin\Artifacts\Debug\NKHook5.dll
echo   bin\Artifacts\Debug\wininet.dll
echo   bin\Artifacts\Release\NKHook5.dll
echo   bin\Artifacts\Release\wininet.dll
popd
exit /b 0

:failed
echo.
echo Repair failed. Re-run this from a Visual Studio 2022 Developer Command Prompt and check the first error above.
popd
exit /b 1
