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

1. Deletes stale `bin\CMakeCache.txt` and `bin\CMakeFiles`
2. Deletes cached FetchContent folders for json_spirit only
3. Reconfigures the Win32 Visual Studio 2022 build
4. Builds `json_spirit`
5. Builds `NKHook5.dll` and `wininet.dll`

Successful output ends with:

```text
Repair complete.
Release artifacts:
  bin\Artifacts\Release\NKHook5.dll
  bin\Artifacts\Release\wininet.dll
```

If `cmake` is not found, open **Developer Command Prompt for VS 2022** and run the script again.

If the link step reports that `bin\_deps\jsonspirit-build\json_spirit\Debug\json_spirit.lib`
or `bin\_deps\jsonspirit-build\json_spirit\Release\json_spirit.lib` cannot be opened,
run `RepairDependencies.bat` again. The script intentionally leaves the Boost cache alone
and only replaces the stale/corrupt json_spirit cache.

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
