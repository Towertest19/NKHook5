#include "LuaScreenBridge.h"

#include <Logging/Logger.h>
#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>

struct lua_State;
using lua_CFunction = int(__cdecl*)(lua_State*);

namespace NKHook5::Patches::Lua
{
    using namespace Common::Logging::Logger;

    namespace
    {
        using lua_pushcclosure_fn = void(__cdecl*)(lua_State*, lua_CFunction, int);
        using lua_pushlightuserdata_fn = void(__cdecl*)(lua_State*, void*);
        using lua_pushstring_fn = const char*(__cdecl*)(lua_State*, const char*);
        using lua_tolstring_fn = const char*(__cdecl*)(lua_State*, int, size_t*);
        using lua_pushboolean_fn = void(__cdecl*)(lua_State*, int);
        using lua_createtable_fn = void(__cdecl*)(lua_State*, int, int);
        using lua_setfield_fn = void(__cdecl*)(lua_State*, int, const char*);
        using lua_getfield_fn = int(__cdecl*)(lua_State*, int, const char*);
        using lua_getglobal_fn = int(__cdecl*)(lua_State*, const char*);
        using lua_setglobal_fn = void(__cdecl*)(lua_State*, const char*);
        using lua_type_fn = int(__cdecl*)(lua_State*, int);
        using lua_touserdata_fn = void*(__cdecl*)(lua_State*, int);
        using lua_settop_fn = void(__cdecl*)(lua_State*, int);
        using lua_pcallk_fn = int(__cdecl*)(lua_State*, int, int, int, ptrdiff_t, void*);

        constexpr int LUA_TNIL = 0;
        constexpr int LUA_TFUNCTION = 6;
        constexpr int LUA_REGISTRYINDEX = -1001000;
        constexpr const char* kBridgeGlobalName = "nkhook_screen_bridge";

        lua_pushcclosure_fn lua_pushcclosure_ptr = nullptr;
        lua_pushlightuserdata_fn lua_pushlightuserdata_ptr = nullptr;
        lua_pushstring_fn lua_pushstring_ptr = nullptr;
        lua_pushboolean_fn lua_pushboolean_ptr = nullptr;
        lua_createtable_fn lua_createtable_ptr = nullptr;
        lua_setfield_fn lua_setfield_ptr = nullptr;
        lua_getfield_fn lua_getfield_ptr = nullptr;
        lua_getglobal_fn lua_getglobal_ptr = nullptr;
        lua_setglobal_fn lua_setglobal_ptr = nullptr;
        lua_type_fn lua_type_ptr = nullptr;
        lua_touserdata_fn lua_touserdata_ptr = nullptr;
        lua_tolstring_fn lua_tolstring_ptr = nullptr;
        lua_settop_fn lua_settop_ptr = nullptr;
        lua_pcallk_fn lua_pcallk_ptr = nullptr;

        std::unordered_set<std::string> kSupportedScreens = {
            "MainMenuScreen",
            "TowerInfoScreen",
            "SpecialtiesScreen",
            "MonkeyLabScreen",
            "SettingsScreen"
        };
        std::unordered_map<std::string, Classes::CBloonsBaseScreen*> g_liveScreens;
        lua_State* g_luaState = nullptr;
        bool g_bridgeInstalled = false;

        void lua_pop(lua_State* L, int n)
        {
            lua_settop_ptr(L, -n - 1);
        }

        void lua_newtable(lua_State* L)
        {
            lua_createtable_ptr(L, 0, 0);
        }

        void lua_pushcfunction(lua_State* L, lua_CFunction fn)
        {
            lua_pushcclosure_ptr(L, fn, 0);
        }

        void lua_setglobal(lua_State* L, const char* name)
        {
            lua_setglobal_ptr(L, name);
        }

        void lua_getglobal(lua_State* L, const char* name)
        {
            lua_getglobal_ptr(L, name);
        }

        std::string ReadScreenName(Classes::CBloonsBaseScreen* screen)
        {
            if (!screen)
                return {};
            try
            {
                return screen->mScreenName;
            }
            catch (...)
            {
                return {};
            }
        }

        bool IsSupportedScreen(const std::string& screenName)
        {
            return kSupportedScreens.find(screenName) != kSupportedScreens.end();
        }

        Classes::CBloonsBaseScreen* GetScreenArg(lua_State* L, int idx)
        {
            if (!lua_touserdata_ptr)
                return nullptr;
            return reinterpret_cast<Classes::CBloonsBaseScreen*>(lua_touserdata_ptr(L, idx));
        }

        int LuaScreenGetName(lua_State* L)
        {
            auto* screen = GetScreenArg(L, 1);
            const std::string name = ReadScreenName(screen);
            lua_pushstring_ptr(L, name.c_str());
            return 1;
        }

        int LuaScreenGetRoot(lua_State* L)
        {
            auto* screen = GetScreenArg(L, 1);
            lua_pushlightuserdata_ptr(L, screen);
            return 1;
        }

        int LuaScreenIs(lua_State* L)
        {
            auto* screen = GetScreenArg(L, 1);
            const char* wanted = lua_tolstring_ptr ? lua_tolstring_ptr(L, 2, nullptr) : nullptr;
            const std::string name = ReadScreenName(screen);
            lua_pushboolean_ptr(L, wanted && name == wanted ? 1 : 0);
            return 1;
        }

        void PushScreenObject(lua_State* L, Classes::CBloonsBaseScreen* screen)
        {
            lua_newtable(L);
            lua_pushlightuserdata_ptr(L, screen);
            lua_setfield_ptr(L, -2, "ptr");
            lua_pushcfunction(L, LuaScreenGetName);
            lua_setfield_ptr(L, -2, "get_name");
            lua_pushcfunction(L, LuaScreenGetRoot);
            lua_setfield_ptr(L, -2, "get_root");
            lua_pushcfunction(L, LuaScreenIs);
            lua_setfield_ptr(L, -2, "is");
        }

        int LuaGetScreen(lua_State* L)
        {
            const char* name = lua_tolstring_ptr ? lua_tolstring_ptr(L, 1, nullptr) : nullptr;
            if (!name)
            {
                lua_pushlightuserdata_ptr(L, nullptr);
                return 1;
            }
            auto it = g_liveScreens.find(name);
            if (it == g_liveScreens.end())
            {
                lua_pushlightuserdata_ptr(L, nullptr);
                return 1;
            }
            PushScreenObject(L, it->second);
            return 1;
        }

        int LuaMakeWidget(lua_State* L)
        {
            lua_newtable(L);
            lua_pushlightuserdata_ptr(L, nullptr);
            lua_setfield_ptr(L, -2, "ptr");
            return 1;
        }

        int LuaAddChild(lua_State* L)
        {
            lua_pushboolean_ptr(L, 1);
            return 1;
        }

        int LuaSetText(lua_State* L)
        {
            lua_pushboolean_ptr(L, 1);
            return 1;
        }

        int LuaSetLabCategory(lua_State* L)
        {
            const char* category = lua_tolstring_ptr ? lua_tolstring_ptr(L, 1, nullptr) : nullptr;
            Print(LogLevel::DEBUG, "Lua ui.set_lab_category('%s') requested", category ? category : "");
            lua_pushboolean_ptr(L, 1);
            return 1;
        }

        void PushUiModule(lua_State* L)
        {
            lua_newtable(L);
            lua_pushcfunction(L, LuaGetScreen);
            lua_setfield_ptr(L, -2, "get_screen");
            lua_pushcfunction(L, LuaMakeWidget);
            lua_setfield_ptr(L, -2, "make_empty");
            lua_pushcfunction(L, LuaMakeWidget);
            lua_setfield_ptr(L, -2, "make_button");
            lua_pushcfunction(L, LuaMakeWidget);
            lua_setfield_ptr(L, -2, "make_text");
            lua_pushcfunction(L, LuaAddChild);
            lua_setfield_ptr(L, -2, "add_child");
            lua_pushcfunction(L, LuaSetText);
            lua_setfield_ptr(L, -2, "set_text");
            lua_pushcfunction(L, LuaSetLabCategory);
            lua_setfield_ptr(L, -2, "set_lab_category");
        }

        int LuaOpenScreenBridge(lua_State* L)
        {
            g_luaState = L;
            lua_newtable(L);
            lua_pushcfunction(L, LuaGetScreen);
            lua_setfield_ptr(L, -2, "get_screen");
            PushUiModule(L);
            lua_setfield_ptr(L, -2, "ui");
            return 1;
        }

        bool ResolveLuaSymbols()
        {
            HMODULE lua = GetModuleHandleA("lua53.dll");
            if (!lua) lua = GetModuleHandleA("lua5.3.dll");
            if (!lua) lua = GetModuleHandleA("lua.dll");
            if (!lua)
                return false;

            lua_pushcclosure_ptr = reinterpret_cast<lua_pushcclosure_fn>(GetProcAddress(lua, "lua_pushcclosure"));
            lua_pushlightuserdata_ptr = reinterpret_cast<lua_pushlightuserdata_fn>(GetProcAddress(lua, "lua_pushlightuserdata"));
            lua_pushstring_ptr = reinterpret_cast<lua_pushstring_fn>(GetProcAddress(lua, "lua_pushstring"));
            lua_pushboolean_ptr = reinterpret_cast<lua_pushboolean_fn>(GetProcAddress(lua, "lua_pushboolean"));
            lua_createtable_ptr = reinterpret_cast<lua_createtable_fn>(GetProcAddress(lua, "lua_createtable"));
            lua_setfield_ptr = reinterpret_cast<lua_setfield_fn>(GetProcAddress(lua, "lua_setfield"));
            lua_getfield_ptr = reinterpret_cast<lua_getfield_fn>(GetProcAddress(lua, "lua_getfield"));
            lua_getglobal_ptr = reinterpret_cast<lua_getglobal_fn>(GetProcAddress(lua, "lua_getglobal"));
            lua_setglobal_ptr = reinterpret_cast<lua_setglobal_fn>(GetProcAddress(lua, "lua_setglobal"));
            lua_type_ptr = reinterpret_cast<lua_type_fn>(GetProcAddress(lua, "lua_type"));
            lua_touserdata_ptr = reinterpret_cast<lua_touserdata_fn>(GetProcAddress(lua, "lua_touserdata"));
            lua_tolstring_ptr = reinterpret_cast<lua_tolstring_fn>(GetProcAddress(lua, "lua_tolstring"));
            lua_settop_ptr = reinterpret_cast<lua_settop_fn>(GetProcAddress(lua, "lua_settop"));
            lua_pcallk_ptr = reinterpret_cast<lua_pcallk_fn>(GetProcAddress(lua, "lua_pcallk"));

            return lua_pushcclosure_ptr && lua_pushlightuserdata_ptr &&
                lua_pushstring_ptr && lua_pushboolean_ptr && lua_createtable_ptr &&
                lua_setfield_ptr && lua_getfield_ptr && lua_getglobal_ptr && lua_setglobal_ptr &&
                lua_type_ptr && lua_touserdata_ptr &&
                lua_tolstring_ptr && lua_settop_ptr && lua_pcallk_ptr;
        }

        bool EnsureBridgeInstalled(lua_State* L)
        {
            if (!L)
                return false;
            if (!lua_pushcclosure_ptr && !ResolveLuaSymbols())
                return false;

            g_luaState = L;
            if (g_bridgeInstalled)
                return true;

            lua_getfield_ptr(L, LUA_REGISTRYINDEX, "_PRELOAD");
            if (lua_type_ptr(L, -1) == LUA_TNIL)
            {
                lua_pop(L, 1);
                return false;
            }
            lua_pushcfunction(L, LuaOpenScreenBridge);
            lua_setfield_ptr(L, -2, "nkhook.screen");
            lua_pop(L, 1);

            lua_getglobal(L, kBridgeGlobalName);
            if (lua_type_ptr(L, -1) == LUA_TNIL)
            {
                lua_pop(L, 1);
                lua_newtable(L);
                lua_pushcfunction(L, LuaGetScreen);
                lua_setfield_ptr(L, -2, "get_screen");
                PushUiModule(L);
                lua_setfield_ptr(L, -2, "ui");
                lua_setglobal(L, kBridgeGlobalName);
            }
            else
            {
                lua_pop(L, 1);
            }

            g_bridgeInstalled = true;
            Print(LogLevel::DEBUG, "Lua screen bridge installed");
            return true;
        }

        void CallLifecycle(const char* callback, Classes::CBloonsBaseScreen* screen)
        {
            if (!g_luaState && !ResolveLuaSymbols())
                return;
            if (!g_luaState)
                return;
            if (!EnsureBridgeInstalled(g_luaState))
                return;

            lua_State* L = g_luaState;
            lua_getglobal(L, "NKHookScreenInjection");
            if (lua_type_ptr(L, -1) == LUA_TNIL)
            {
                lua_pop(L, 1);
                return;
            }
            lua_getfield_ptr(L, -1, callback);
            if (lua_type_ptr(L, -1) != LUA_TFUNCTION)
            {
                lua_pop(L, 2);
                return;
            }
            PushScreenObject(L, screen);
            if (lua_pcallk_ptr(L, 1, 0, 0, 0, nullptr) != 0)
            {
                const char* err = lua_tolstring_ptr(L, -1, nullptr);
                Print(LogLevel::ERR, "Lua screen callback %s failed: %s", callback, err ? err : "unknown error");
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }

    extern "C" __declspec(dllexport) int __cdecl luaopen_nkhook_screen(lua_State* L)
    {
        if (!lua_pushcclosure_ptr)
            ResolveLuaSymbols();
        g_luaState = L;
        return LuaOpenScreenBridge(L);
    }

    extern "C" __declspec(dllexport) void __cdecl NKHookRegisterLuaState(lua_State* L)
    {
        EnsureBridgeInstalled(L);
    }

    bool LuaScreenBridge::Apply()
    {
        if (!ResolveLuaSymbols())
        {
            Print(LogLevel::DEBUG, "Lua screen bridge: Lua 5.3 exports not loaded yet; module export remains available");
        }
        return true;
    }

    void NotifyScreenInit(Classes::CBloonsBaseScreen* screen)
    {
        const std::string name = ReadScreenName(screen);
        if (!IsSupportedScreen(name))
            return;
        g_liveScreens[name] = screen;
        CallLifecycle("on_init", screen);
    }

    void NotifyScreenUpdate(Classes::CBloonsBaseScreen* screen)
    {
        const std::string name = ReadScreenName(screen);
        if (!IsSupportedScreen(name))
            return;
        g_liveScreens[name] = screen;
        CallLifecycle("on_update", screen);
    }

    void NotifyScreenCleanup(Classes::CBloonsBaseScreen* screen)
    {
        const std::string name = ReadScreenName(screen);
        if (!IsSupportedScreen(name))
            return;
        CallLifecycle("on_cleanup", screen);
        g_liveScreens.erase(name);
    }
}
