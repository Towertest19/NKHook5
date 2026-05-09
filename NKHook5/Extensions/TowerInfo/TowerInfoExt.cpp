#include "TowerInfoExt.h"

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
		
		// CanBeViewed flag - controls visibility in tower info panel
		// Optional. If not specified, we decide at query-time based on isCustomTower.
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
			def.towerType.c_str(), 
			def.canBeViewed ? "true" : "false",
			def.canBeViewedSpecified ? "" : "/unspecified",
			def.hideUpgradeUnlocks ? "true" : "false");
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

		// Otherwise: vanilla always visible; custom hidden by default.
		return isCustomTower ? false : true;
	}
	
	// No definition found
	return isCustomTower ? false : true;
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
