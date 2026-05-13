#include "SetTower.h"

#include "../../Extensions/ExtensionManager.h"
#include "../../Extensions/TowerInfo/TowerInfoExt.h"
#include "../../Signatures/Signature.h"
#include "../../Util/FlagManager.h"
#include "../../Utils.h"
#include <Logging/Logger.h>
#include <polyhook2/Detour/x86Detour.hpp>

#include <cstdint>
#include <cstring>

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

	static constexpr size_t kPopulatedFlagOffset = 0x198;
	static constexpr size_t kTailJumpOffset = 0x15;
	static uint64_t o_func;
	static uintptr_t o_setTowerImpl;

	void __fastcall cb_hook_settower(void* thisptr, int pad, uint64_t towerId);

	static uintptr_t ResolveSetTowerImpl(const void* wrapperAddress)
	{
		const auto* bytes = reinterpret_cast<const uint8_t*>(wrapperAddress);
		if (bytes[kTailJumpOffset] != 0xE9)
		{
			Print(LogLevel::WARNING, "TowerInfoScreen SetTower patch: wrapper tail jump not found at %p", wrapperAddress);
			return 0;
		}

		int32_t rel = 0;
		memcpy(&rel, bytes + kTailJumpOffset + 1, sizeof(rel));
		return reinterpret_cast<uintptr_t>(wrapperAddress) + kTailJumpOffset + 5 + rel;
	}

	static void CallSetTower(void* thisptr, int pad, uint64_t towerId)
	{
		auto wrapper = reinterpret_cast<void(__fastcall*)(void*, int, uint64_t)>(o_func);

		// The tiny wrapper at TowerInfoScreen_SetTower only checks the low 32 bits
		// before it tail-calls the real implementation. Big tower flags after
		// GameDummy have a zero low dword, so bypass the wrapper for those IDs.
		if (towerId != 0 && static_cast<uint32_t>(towerId) == 0 && o_setTowerImpl != 0 &&
			Utils::IsAddressExecutable(o_setTowerImpl))
		{
			*reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(thisptr) + kPopulatedFlagOffset) = 1;
			auto impl = reinterpret_cast<void(__thiscall*)(void*, uint64_t)>(o_setTowerImpl);
			impl(thisptr, towerId);
			return;
		}

		wrapper(thisptr, pad, towerId);
	}

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

			o_setTowerImpl = ResolveSetTowerImpl(address);
			if (o_setTowerImpl != 0)
			{
				Print(LogLevel::INFO, "TowerInfoScreen SetTower patch: resolved implementation at %p", reinterpret_cast<void*>(o_setTowerImpl));
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
		// Empty tower ID: clear the info panel.
		if (towerId == 0)
		{
			CallSetTower(thisptr, pad, 0);
			return;
		}

		auto* towerInfoExt = ExtensionManager::Get<TowerInfoExt>();
		std::string towerName = g_towerFlags.GetName(towerId);
		const bool registeredByFlags = !towerName.empty() && towerName != "INVALID";
		const bool registeredByTowerInfo = towerInfoExt && towerInfoExt->GetDefinition(towerId) != nullptr;

		if (!registeredByFlags && !registeredByTowerInfo)
		{
			// Not a custom tower (or flag manager not yet populated) - vanilla path.
			CallSetTower(thisptr, pad, towerId);
			return;
		}

		if (!registeredByFlags && registeredByTowerInfo)
		{
			const auto* def = towerInfoExt->GetDefinition(towerId);
			towerName = def ? def->towerType : "INVALID";
		}

		bool shouldDisplay = true;
		if (towerInfoExt)
		{
			shouldDisplay = towerInfoExt->ShouldDisplayInInfoPanel(towerId, towerName, true);
		}

		if (!shouldDisplay)
		{
			Print(LogLevel::INFO, "TowerInfoScreen: Hiding custom tower '%s' (ID: %llu) from info panel",
				towerName.c_str(), towerId);
			CallSetTower(thisptr, pad, 0);
			return;
		}

		Print(LogLevel::INFO, "TowerInfoScreen: Displaying custom tower '%s' (ID: %llu)",
			towerName.c_str(), towerId);
		CallSetTower(thisptr, pad, towerId);
	}
}
