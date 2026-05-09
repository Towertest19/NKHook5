#include "SetTower.h"

#include "../../Extensions/ExtensionManager.h"
#include "../../Extensions/TowerInfo/TowerInfoExt.h"
#include "../../Signatures/Signature.h"
#include "../../Util/FlagManager.h"
#include "../../Utils.h"
#include <Logging/Logger.h>
#include <polyhook2/Detour/x86Detour.hpp>

extern NKHook5::Util::FlagManager g_towerFlags;

namespace NKHook5::Patches::TowerInfoScreen
{
	using namespace Common;
	using namespace Common::Extensions;
	using namespace Common::Logging;
	using namespace Common::Logging::Logger;
	using namespace NKHook5;
	using namespace NKHook5::Extensions;
	using namespace NKHook5::Extensions::TowerInfo;
	using namespace Signatures;

	static uint64_t o_func;
	
	void __fastcall cb_hook_settower(void* thisptr, int pad, uint64_t towerId);
	
	bool SetTower::Apply()
	{
		const void* address = Signatures::GetAddressOf(Sigs::TowerInfoScreen_SetTower);
		if (address)
		{
			if (!Utils::IsAddressExecutable((size_t)address))
			{
				Print(LogLevel::ERR, "TowerInfoScreen SetTower patch: Resolved address %p is not executable; refusing to hook", address);
				return false;
			}

			Print(LogLevel::INFO, "TowerInfoScreen SetTower patch: Found function at %p, applying hook...", address);
			auto* detour = new PLH::x86Detour((const uintptr_t)address, (uintptr_t)&cb_hook_settower, &o_func);
			if (detour->hook())
			{
				Print(LogLevel::INFO, "TowerInfoScreen SetTower patch: Successfully hooked! Tower visibility control active.");
				return true;
			}
			else
			{
				Print(LogLevel::ERR, "TowerInfoScreen SetTower patch: Failed to hook function at %p", address);
				return false;
			}
		}
		else
		{
			Print(LogLevel::WARNING, "TowerInfoScreen SetTower patch: Could not find function signature");
			return false;
		}
	}

	void __fastcall cb_hook_settower(void* thisptr, int pad, uint64_t towerId)
	{
		auto ofn = (void(__fastcall*)(void*, int, uint64_t))o_func;

		// If called before IDs/types are injected, we may see placeholder/garbage values.
		// Only act on values that look like real tower flags.
		if (towerId == 0)
		{
			ofn(thisptr, pad, towerId);
			return;
		}

		// We only support the flag-based tower IDs here (power-of-two bit flags).
		// If it's not a flag, pass through.
		if ((towerId & (towerId - 1)) != 0)
		{
			ofn(thisptr, pad, towerId);
			return;
		}

		// Only process custom towers - vanilla towers should always pass through
		// Custom towers typically have IDs beyond GameDummy (bit 58)
		if (towerId <= (1ull << 58))
		{
			ofn(thisptr, pad, towerId);
			return;
		}
		
		// This is a custom tower, check if it should be displayed
		std::string towerName = g_towerFlags.GetName(towerId);
		if (towerName.empty() || towerName == "INVALID")
		{
			// Flag manager not ready / not injected yet, or unknown flag: do nothing.
			ofn(thisptr, pad, towerId);
			return;
		}
		
		// Default to displaying custom towers unless explicitly told not to
		bool shouldDisplay = true;
		
		if (!towerName.empty())
		{
			auto* towerInfoExt = ExtensionManager::Get<TowerInfoExt>();
			if (towerInfoExt)
			{
				shouldDisplay = towerInfoExt->ShouldDisplayInInfoPanel(towerName, true); // true = isCustomTower
				
				if (!shouldDisplay)
				{
					Print(LogLevel::INFO, "TowerInfoScreen: Hiding custom tower '%s' (ID: %llu) from info panel", 
						towerName.c_str(), towerId);
					
					// Call original function with invalid ID to prevent display
					ofn(thisptr, pad, 0); // Pass 0 to hide tower
					return;
				}
			}
		}
		
		// Call original function for custom tower
		ofn(thisptr, pad, towerId);
	}
}
