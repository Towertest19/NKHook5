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

		// Empty tower ID: clear the info panel.
		if (towerId == 0)
		{
			ofn(thisptr, pad, 0);
			return;
		}

		// Resolve this ID against the custom tower flag manager.
		// Any ID that is NOT in g_towerFlags is a vanilla/engine tower - pass it
		// straight through so the game handles display and unlock popups normally.
		//
		// We intentionally skip the IsBitFlag() / power-of-two guard that was here
		// before: custom towers that fell back to numeric IDs (when bit-flag
		// slots were exhausted) are still registered in g_towerFlags and must
		// go through the TowerInfoExt visibility check just like bit-flag towers.
		std::string towerName = g_towerFlags.GetName(towerId);
		if (towerName.empty() || towerName == "INVALID")
		{
			// Not a custom tower (or flag manager not yet populated) - vanilla path.
			ofn(thisptr, pad, towerId);
			return;
		}

		// Custom tower: consult TowerInfoExt for the visibility decision.
		// Default to showing the tower when the extension is not yet loaded so
		// that valid custom towers still trigger the vanilla unlock popup that
		// the original SetTower implementation fires.
		bool shouldDisplay = true;
		auto* towerInfoExt = ExtensionManager::Get<TowerInfoExt>();
		if (towerInfoExt)
		{
			shouldDisplay = towerInfoExt->ShouldDisplayInInfoPanel(towerName, true);
		}

		if (!shouldDisplay)
		{
			Print(LogLevel::INFO, "TowerInfoScreen: Hiding custom tower '%s' (ID: %llu) from info panel",
				towerName.c_str(), towerId);
			// Pass 0 so the original wrapper exits via its early-return path,
			// leaving the info panel cleared rather than showing stale data.
			ofn(thisptr, pad, 0);
			return;
		}

		// Show the custom tower: forward the real ID so the original wrapper
		// sets the populate-flag, tail-calls the display logic, and fires the
		// same unlock popup mechanism used for vanilla towers.
		Print(LogLevel::INFO, "TowerInfoScreen: Displaying custom tower '%s' (ID: %llu)",
			towerName.c_str(), towerId);
		ofn(thisptr, pad, towerId);
	}
}
