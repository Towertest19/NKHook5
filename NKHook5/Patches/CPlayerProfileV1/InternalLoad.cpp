#include "InternalLoad.h"

#include "../../Classes/CPlayerProfileV1.h"
#include "../../Mod/SaveData.h"
#include "../../Signatures/Signature.h"
#include "../../Util/FlagManager.h"

#include <Logging/Logger.h>

#include <unordered_set>

extern NKHook5::Util::FlagManager g_towerFlags;

namespace NKHook5
{
    namespace Patches
    {
        namespace CPlayerProfileV1
        {
            using namespace Mod;
            using namespace Signatures;
            using namespace Common;
            using namespace Common::Logging;
            using namespace Common::Logging::Logger;

            static uint64_t o_func;
            bool __fastcall cb_hook(Classes::CPlayerProfileV1* profile, int pad, class CBaseFileIO* pFileIO, nfw::string fileName, bool param_3) {
                bool result = PLH::FnCast(o_func, &cb_hook)(profile, pad, pFileIO, fileName, param_3);
                /*SaveData* customData = SaveData::GetInstance();
                customData->Load("./Modded.save");*/
                //Add all towers to the profile
                Print("Adding all towers to save...");
                const auto& allTowerFlags = g_towerFlags.GetAll();
                std::unordered_set<std::string> unlockedNames;
                for (const auto& [flag, str] : allTowerFlags) {
                    if (g_towerFlags.IsVanilla(flag) || str.empty() || str == "INVALID")
                        continue;
                    if (!unlockedNames.insert(str).second)
                        continue;
                    if (!Util::FlagManager::IsBitFlag(flag)) {
                        const uint64_t canonical = g_towerFlags.GetFlag(str);
                        if (canonical != 0 && Util::FlagManager::IsBitFlag(canonical))
                            continue;
                    }
                    profile->towerUnlocks[flag] = true;
                    Print("Added tower '%s' with ID '%llx' to save", str.c_str(), flag);
                }
                Print("Done!");
                return result;
            }

            auto InternalLoad::Apply() -> bool
            {
                const void* address = Signatures::GetAddressOf(Sigs::CPlayerProfileV1_InternalLoad);
                if (address)
                {
                    PLH::x86Detour* detour = new PLH::x86Detour((const uint64_t)address, (const uintptr_t)&cb_hook, &o_func);
                    if (detour->hook())
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
            }
        }
    }
}