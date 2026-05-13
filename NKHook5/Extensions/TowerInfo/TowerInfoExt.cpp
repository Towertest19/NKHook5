#include "TowerInfoExt.h"

#include "../../Util/FlagManager.h"

#include <Logging/Logger.h>

using namespace Common;
using namespace Common::Extensions;
using namespace Common::Logging::Logger;
using namespace NKHook5;
using namespace NKHook5::Extensions;
using namespace NKHook5::Extensions::TowerInfo;

TowerInfoExt::TowerInfoExt() : JsonExtension("TowerInfo", "Assets/JSON/TowerDefinitions/*.tower")
{
}

void TowerInfoExt::UseJsonData(nlohmann::json content)
{
	if (content.empty())
	{
		Print(LogLevel::ERR, "Received empty json data for a TowerInfo definition");
		return;
	}

	if (!content.contains("TypeName"))
	{
		Print(LogLevel::ERR, "Received a TowerDefinition without a 'TypeName' property!");
		return;
	}

	try
	{
		TowerInfoDefinition def;
		def.towerType = content["TypeName"].get<std::string>();
		loadedAny = true;

		// CanBeViewed flag - controls visibility in tower info panel.
		// Optional. If not specified, custom towers are visible by default so
		// unlock and level-up screens can be populated for modded IDs.
		if (content.contains("CanBeViewed"))
		{
			def.canBeViewedSpecified = true;
			def.canBeViewed = content["CanBeViewed"].get<bool>();
		}

		// HideUpgradeUnlocks flag - prevents XP unlock notifications
		if (content.contains("HideUpgradeUnlocks"))
		{
			def.hideUpgradeUnlocks = content["HideUpgradeUnlocks"].get<bool>();
		}

		// Custom description for info panel
		if (content.contains("InfoDescription"))
		{
			def.customDescription = content["InfoDescription"].get<std::string>();
		}

		// Store in map for quick lookup
		nameToIndex[def.towerType] = definitions.size();
		definitions.emplace_back(std::move(def));

		Print(LogLevel::INFO, "Loaded TowerDefinition: '%s' (CanBeViewed=%s%s, HideUpgradeUnlocks=%s)",
			definitions.back().towerType.c_str(),
			definitions.back().canBeViewed ? "true" : "false",
			definitions.back().canBeViewedSpecified ? "" : "/unspecified",
			definitions.back().hideUpgradeUnlocks ? "true" : "false");
	}
	catch (const std::exception& e)
	{
		Print(LogLevel::ERR, "Failed to parse TowerInfo JSON: %s", e.what());
	}
}

const std::vector<TowerInfoDefinition>& TowerInfoExt::GetDefinitions() const
{
	return this->definitions;
}

const TowerInfoDefinition* TowerInfoExt::GetDefinition(const std::string& towerType) const
{
	auto it = nameToIndex.find(towerType);
	if (it != nameToIndex.end())
	{
		return &definitions[it->second];
	}
	return nullptr;
}

const TowerInfoDefinition* TowerInfoExt::GetDefinition(uint64_t towerId) const
{
	auto it = idToIndex.find(towerId);
	if (it != idToIndex.end())
	{
		return &definitions[it->second];
	}
	return nullptr;
}

void TowerInfoExt::FinalizeTowerRegistration(const Util::FlagManager& towerFlags)
{
	idToIndex.clear();
	for (size_t i = 0; i < definitions.size(); ++i)
	{
		auto& def = definitions[i];
		def.towerId = towerFlags.GetFlag(def.towerType);
		if (def.towerId != 0)
		{
			idToIndex[def.towerId] = i;
			Print(LogLevel::INFO, "TowerInfo: registered '%s' as tower ID %llu", def.towerType.c_str(), def.towerId);
		}
		else
		{
			Print(LogLevel::WARNING, "TowerInfo: tower '%s' has no registered tower ID yet", def.towerType.c_str());
		}
	}
}

bool TowerInfoExt::ShouldDisplayInInfoPanel(const std::string& towerType, bool isCustomTower) const
{
	if (towerType.empty() || towerType == "INVALID")
	{
		return true;
	}

	// If tower definitions haven't been loaded yet, don't make restrictive decisions.
	if (!loadedAny)
	{
		return true;
	}

	const TowerInfoDefinition* def = GetDefinition(towerType);
	if (def)
	{
		// If the definition explicitly specifies CanBeViewed, respect it.
		if (def->canBeViewedSpecified)
		{
			return def->canBeViewed;
		}

		// Otherwise: vanilla and custom towers are visible by default.
		return true;
	}

	// Unknown custom towers are also visible by default; this keeps the level-up
	// and unlock flow from silently dropping tower IDs that were registered after
	// GameDummy or assigned fallback IDs.
	return true;
}

bool TowerInfoExt::ShouldDisplayInInfoPanel(uint64_t towerId, const std::string& towerType, bool isCustomTower) const
{
	if (const TowerInfoDefinition* def = GetDefinition(towerId))
	{
		return !def->canBeViewedSpecified || def->canBeViewed;
	}

	return ShouldDisplayInInfoPanel(towerType, isCustomTower);
}

bool TowerInfoExt::ShouldHideUpgradeUnlocks(const std::string& towerType) const
{
	const TowerInfoDefinition* def = GetDefinition(towerType);
	if (def)
	{
		return def->hideUpgradeUnlocks;
	}
	return false; // Default: show upgrade unlocks
}
