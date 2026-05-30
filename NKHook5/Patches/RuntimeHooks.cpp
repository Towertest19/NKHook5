#include "RuntimeHooks.h"

#include "../Extensions/LabDefinitions/LabDefinitionsExt.h"
#include "../Extensions/SpecialtyDefinitions/SpecialtyDefinitionsExt.h"
#include "../Extensions/TowerInfo/TowerInfoExt.h"

#include <Extensions/ExtensionManager.h>
#include <Logging/Logger.h>

namespace NKHook5::Patches::RuntimeHooks
{
	using namespace Common::Extensions;
	using namespace Common::Logging::Logger;
	using namespace NKHook5::Extensions;
	using namespace NKHook5::Extensions::LabDefinitions;
	using namespace NKHook5::Extensions::SpecialtyDefinitions;
	using namespace NKHook5::Extensions::TowerInfo;

	namespace
	{
		bool g_towerTypesReady = false;
		bool g_labSpecialtyPrimed = false;
		bool g_towerInfoPrimed = false;
	}

	void PrimeLabSpecialtyAfterMods()
	{
		if (g_labSpecialtyPrimed)
			return;
		g_labSpecialtyPrimed = true;

		Print(LogLevel::DEBUG,
			"RuntimeHooks: mods registered; priming lab/specialty metadata from merged NKH.");

		if (auto* labExt = ExtensionManager::Get<LabDefinitionsExt>())
			labExt->PreloadRuntime();
		if (auto* specialtyExt = ExtensionManager::Get<SpecialtyDefinitionsExt>())
			specialtyExt->PreloadRuntime();
	}

	void NotifyTowerTypesReady()
	{
		if (g_towerTypesReady)
			return;

		g_towerTypesReady = true;
		Print(LogLevel::DEBUG, "RuntimeHooks: tower types ready; priming towerinfo from merged NKH.");

		if (g_towerInfoPrimed)
			return;
		g_towerInfoPrimed = true;

		if (auto* towerInfoExt = ExtensionManager::Get<TowerInfoExt>())
		{
			towerInfoExt->PreloadRuntime();
		}
	}

	void OnMergedAssetLoaded(const std::string& /*assetPath*/)
	{
		if (!g_labSpecialtyPrimed)
			PrimeLabSpecialtyAfterMods();
	}
}
