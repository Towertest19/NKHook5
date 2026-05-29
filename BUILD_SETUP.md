# NKHook5 build setup and dependency repair

NKHook5 is a 32-bit Visual Studio CMake project. Build it from Windows with Visual Studio 2022 installed.

## Requirements

- Visual Studio 2022 with **Desktop development with C++**
- CMake support for Visual Studio
- Git available in `PATH`

## Repair missing or broken dependencies

Run this from the repository root:

```bat
RepairDependencies.bat
```

The repair script:

1. Deletes stale `bin\CMakeCache.txt`, `bin\CMakeFiles`, generated CMake project files, and NKHook5/Loader/Common build folders
2. Deletes cached FetchContent folders for json_spirit only
3. Reconfigures the Win32 Visual Studio 2022 build
4. Uses json_spirit header-only mode for NKHook5, so no `json_spirit.lib` is required
5. Builds `NKHook5.dll` and `wininet.dll` in Debug and Release

Successful output ends with:

```text
Repair complete.
Artifacts:
  bin\Artifacts\Debug\NKHook5.dll
  bin\Artifacts\Debug\wininet.dll
  bin\Artifacts\Release\NKHook5.dll
  bin\Artifacts\Release\wininet.dll
```

If `cmake` is not found, open **Developer Command Prompt for VS 2022** and run the script again.

If the link step reports that `bin\_deps\jsonspirit-build\json_spirit\Debug\json_spirit.lib`
or `bin\_deps\jsonspirit-build\json_spirit\Release\json_spirit.lib` cannot be opened,
make sure this branch is checked out, then run `RepairDependencies.bat` again. The script
intentionally leaves the Boost cache alone and removes stale generated CMake project files
that may still point at the old json_spirit DLL/import-library target.

## Normal builds after repair

After dependencies are repaired, use:

```bat
BuildFast.bat
```

For a full clean Debug + Release build, use:

```bat
BuildAll.bat
```

## Installing a local build

Copy these files into the Bloons TD 5 game directory:

```text
bin\Artifacts\Release\wininet.dll
bin\Artifacts\Release\NKHook5.dll
```

Create a `Mods` folder next to the game executable if it does not already exist.
