#include <Windows.h>
#include <string>
#include <iostream>
#include <vector>

#include <Extensions/ExtensionManager.h>
#include <Logging/Logger.h>

#include "Extensions/StatusEffect/StatusDefinitionsExt.h"
#include "Extensions/LabDefinitions/LabDefinitionsExt.h"
#include "Extensions/SpecialtyDefinitions/SpecialtyDefinitionsExt.h"
#include "Extensions/TowerInfo/TowerInfoExt.h"
#include "Patches/PatchManager.h"
#include "Signatures/Signature.h"

std::string* testModDir = nullptr;

using namespace Common;
using namespace Common::Logging;
using namespace Common::Logging::Logger;

static std::string GetEnvVar(const char* name)
{
        DWORD needed = GetEnvironmentVariableA(name, nullptr, 0);
        if (needed == 0)
        {
                return {};
        }
        std::vector<char> buf(needed);
        DWORD written = GetEnvironmentVariableA(name, buf.data(), needed);
        if (written == 0)
        {
                return {};
        }
        return std::string(buf.data());
}

static void AppendLuaSearchPath(const char* varName, const std::string& prefixPaths)
{
        std::string current = GetEnvVar(varName);
        std::string next;
        if (current.empty())
        {
                next = prefixPaths;
        }
        else
        {
                next = prefixPaths + ";" + current;
        }
        if (SetEnvironmentVariableA(varName, next.c_str()))
        {
                Print(LogLevel::INFO, "Lua env: set %s='%s'", varName, next.c_str());
        }
        else
        {
                Print(LogLevel::WARNING, "Lua env: failed to set %s (err=%lu)", varName, (unsigned long)GetLastError());
        }
}

int initialize() {
#ifdef _DEBUG
    Print(LogLevel::INFO, "Press enter to launch NKH...");
    std::cin.get();
#endif
    Print(LogLevel::INFO, "Loading NKHook5...");

    wchar_t* cmdLine = GetCommandLineW();
    int argc;
    wchar_t** argv = CommandLineToArgvW(cmdLine, &argc);

    bool wantsModLaunch = false;
    wchar_t* wcModDir = nullptr;
    if (argv) {
        for (int i = 0; i < argc; i++) {
            if (wantsModLaunch)
            {
                wcModDir = argv[i];
                wantsModLaunch = false;
                continue;
            }

            if (lstrcmpW(argv[i], L"--LaunchMod") == 0)
            {
                wantsModLaunch = true;
                continue;
            }

            wprintf(L"Unknown arg at %d: %s\n", i, argv[i]);
        }
    }
    if (wantsModLaunch && !wcModDir)
    {
        Print(LogLevel::WARNING, "--LaunchMod provided without a path");
    }
    if (wcModDir) {
        int size = WideCharToMultiByte(CP_UTF8, 0, wcModDir, -1, nullptr, 0, nullptr, nullptr);
        testModDir = new std::string(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wcModDir, -1, &(*testModDir)[0], size, nullptr, nullptr);
        Print(LogLevel::INFO, "Launching mod: %s", testModDir->c_str());

        std::string modDir = *testModDir;
        for (auto& ch : modDir)
        {
            if (ch == '\\') ch = '/';
        }

        // Prefer scripts in the launched mod directory.
        // Typical Lua package patterns; we prepend ours so it wins over vanilla.
        std::string luaPathPrefix =
            modDir + "/Scripts/?.lua;" +
            modDir + "/Scripts/?/init.lua;" +
            modDir + "/Assets/Scripts/?.lua;" +
            modDir + "/Assets/Scripts/?/init.lua";
        AppendLuaSearchPath("LUA_PATH_5_3", luaPathPrefix);

        // Optional native module search (if the game uses it)
        std::string luaCPathPrefix =
            modDir + "/Scripts/?.dll;" +
            modDir + "/Assets/Scripts/?.dll";
        AppendLuaSearchPath("LUA_CPATH_5_3", luaCPathPrefix);
    }

    Print(LogLevel::INFO, "Searching signatures...");
    NKHook5::Signatures::FindAll();
    Print(LogLevel::INFO, "Search complete! (Please report any search issues!!!)");

    Print(LogLevel::INFO, "Loading Extensions...");
    Common::Extensions::ExtensionManager::AddAll();
    Common::Extensions::ExtensionManager::AddExtension<NKHook5::Extensions::StatusEffect::StatusDefinitionsExt>();
    // TowerInfo must be queued before lab/specialty metadata so custom tower
    // registration immediately has its visibility/unlock metadata available.
    Common::Extensions::ExtensionManager::AddExtension<NKHook5::Extensions::TowerInfo::TowerInfoExt>();
    Common::Extensions::ExtensionManager::AddExtension<NKHook5::Extensions::LabDefinitions::LabDefinitionsExt>();
    Common::Extensions::ExtensionManager::AddExtension<NKHook5::Extensions::SpecialtyDefinitions::SpecialtyDefinitionsExt>();
    Print(LogLevel::INFO, "All extensions loaded!");

    Print(LogLevel::INFO, "Loading all patches...");
    NKHook5::Patches::PatchManager::ApplyAll();
    Print(LogLevel::INFO, "Patch loading complete! (Some patches may have failed - check logs for details)");

    Print(LogLevel::INFO, "Loaded NKHook5!");
#ifdef _DEBUG
    Print(LogLevel::INFO, "Press enter to boot game...");
    std::cin.get();
#endif

    return 0;
}


extern "C" __declspec(dllexport) bool __stdcall DllMain(
    HINSTANCE hinstDLL,  // handle to DLL module
    DWORD fdwReason,     // reason for calling function
    LPVOID lpReserved )  // reserved
{
    // Perform actions based on the reason for calling.
    switch( fdwReason ) 
    { 
        case DLL_PROCESS_ATTACH:
            initialize();
            break;
    }
    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}