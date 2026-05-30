#include "InternalLoad.h"

#include "../../Classes/CPlayerProfileV1.h"
#include "../../Extensions/ExtensionManager.h"
#include "../../Extensions/TowerInfo/TowerInfoExt.h"
#include "../../Mod/SaveData.h"
#include "../../Signatures/Signature.h"
#include "../../Util/FlagManager.h"

#include <Logging/Logger.h>

#include <cstdint>
#include <unordered_set>

#include <string>

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
            using namespace Common::Extensions;
            using namespace Extensions::TowerInfo;
            using namespace Common::Logging;
            using namespace Common::Logging::Logger;

            static uint64_t o_func;

            namespace
            {
                constexpr uint64_t kGameDummyFlag = 1ull << 58ull;

                bool IsSaveRegistrationExcluded(uint64_t flag, const std::string& towerType)
                {
                    return flag == kGameDummyFlag || towerType == "GameDummy";
                }
            }
            bool __fastcall cb_hook(Classes::CPlayerProfileV1* profile, int pad, class CBaseFileIO* pFileIO, nfw::string fileName, bool param_3) {
                bool result = PLH::FnCast(o_func, &cb_hook)(profile, pad, pFileIO, fileName, param_3);
                /*SaveData* customData = SaveData::GetInstance();
                customData->Load("./Modded.save");*/
                // Add custom towers to the profile without touching vanilla helper slots.
                Print(LogLevel::DEBUG, "Adding custom towers to save...");
                const auto& allTowerFlags = g_towerFlags.GetAll();
                std::unordered_set<std::string> unlockedNames;
                auto* towerInfoExt = ExtensionManager::Get<TowerInfoExt>();
                for (const auto& [flag, str] : allTowerFlags) {
                    if (str.empty() || str == "INVALID" || IsSaveRegistrationExcluded(flag, str))
                        continue;
                    if (flag < (1ull << 59) && !Util::FlagManager::IsCustomFallbackId(flag))
                        continue;
                    if (!unlockedNames.insert(str).second)
                        continue;
                    if (!Util::FlagManager::IsBitFlag(flag)) {
                        const uint64_t canonical = g_towerFlags.GetFlag(str);
                        if (canonical != 0 && Util::FlagManager::IsBitFlag(canonical))
                            continue;
                    }
                    if (towerInfoExt) {
                        towerInfoExt->BindDefinitionId(str, flag);
                        if (!towerInfoExt->CanUnlockTower(flag, str)) {
                            profile->towerUnlocks[flag] = false;
                            profile->unlockedLevel4[flag] = false;
                            Print(LogLevel::DEBUG, "Tower '%s' with ID '%llx' left locked by TowerInfo CanBeUnlocked=false", str.c_str(), flag);
                            continue;
                        }
                    }
                    profile->towerUnlocks[flag] = true;
                    Print(LogLevel::DEBUG, "Added tower '%s' with ID '%llx' to save", str.c_str(), flag);
                }
                Print(LogLevel::DEBUG, "Custom tower save registration done.");
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