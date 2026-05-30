#include "TowerInfoExt.h"

#include "../../Util/FlagManager.h"
#include "../../Util/JetJsonLoader.h"


#include <Extensions/ExtensionManager.h>
#include <Logging/Logger.h>

#include <unordered_set>

using namespace Common;
using namespace Common::Extensions;
using namespace Common::Logging::Logger;
using namespace NKHook5;
using namespace NKHook5::Extensions;
using namespace NKHook5::Extensions::TowerInfo;
using namespace NKHook5::Util;

namespace
{
	bool IsTowerInfoExcludedName(const std::string& towerType)
	{
		static const std::unordered_set<std::string> excluded = {
			"",
			"INVALID",
			"TestTower",
			"RoadSpikes",
			"ExplodingPineapple",
			"MeerkatSpyPro",
			"MeerkatSpy",
			"TribalTurtlePro",
			"TribalTurtle",
			"PortableLakePro",
			"PortableLake",
			"PontoonPro",
			"Pontoon",
			"BloonsdayDevicePro",
			"BloonsdayDevice",
			"AngrySquirrelPro",
			"AngrySquirrel",
			"SuperMonkeyStormPro",
			"SuperMonkeyStorm",
			"BeeKeeperPro",
			"BeeKeeper",
			"BloonberryBushPro",
			"BloonberryBush",
			"RadadactylPro",
			"Radadactyl",
			"BananaFarmerPro",
			"BananaFarmer",
			"WizardLord",
			"AcePlane",
			"AircraftCarrier",
			"PhoenixPlane",
			"SuperPhoenixPlane",
			"SupplyDropPlane",
			"HeliPilotAircraft",
			"RadadactylPlane",
			"RadderdactylPlane",
			"MonkeyEngineerSentry",
			"MonkeyEngineerSentryTier4",
			"GameDummy",
		};
		return excluded.contains(towerType);
	}

	bool ReadTowerInfoToggles(const nlohmann::json& json, TowerInfoDefinition& def)
	{
		def.canBeViewed = json.value("CanBeViewed", true);
		def.canBeViewedSpecified = json.contains("CanBeViewed");

		if (json.contains("CanBeUnlocked"))
			def.canBeUnlocked = json.value("CanBeUnlocked", true);
		else if (json.contains("HideUpgradeUnlocks"))
			def.canBeUnlocked = !json.value("HideUpgradeUnlocks", false);

		if (json.contains("InfoDescription"))
			def.customDescription = json.value("InfoDescription", "");

		return true;
	}
}

TowerInfoExt::TowerInfoExt() : JsonExtension("TowerInfo", "*/Assets/JSON/TowerDefinitions/*.tower")
{
}

void TowerInfoExt::UseJsonData(nlohmann::json content)
{
	if (content.is_array())
	{
		for (const auto& item : content)
			UseJsonData(item);
		return;
	}

	if (!content.is_object() || content.empty())
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
		if (IsTowerInfoExcludedName(def.towerType))
			return;
		loadedAny = true;

		ReadTowerInfoToggles(content, def);

		// Store in map for quick lookup
		nameToIndex[def.towerType] = definitions.size();
		definitions.emplace_back(std::move(def));

		Print(LogLevel::INFO, "Loaded TowerDefinition: '%s' (CanBeViewed=%s%s, CanBeUnlocked=%s)",
			definitions.back().towerType.c_str(),
			definitions.back().canBeViewed ? "true" : "false",
			definitions.back().canBeViewedSpecified ? "" : "/unspecified",
			definitions.back().canBeUnlocked ? "true" : "false");
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

void TowerInfoExt::PreloadRuntime()
{
	if (runtimePreloaded)
		return;

	Print(LogLevel::INFO, "Hijacking tower info runtime to preload merged .nkh/.jet metadata...");
	Print(LogLevel::INFO, "Copying vanilla tower info definitions...");
	PreloadJsonExtension(*this);
	firstCustomDefinitionIndex = definitions.size();
	Print(LogLevel::INFO, "Old tower info definitions copied; mod tower metadata will bind from tower flags.");

	firstCustomDefinitionIndex = std::min(firstCustomDefinitionIndex, definitions.size());
	Print(LogLevel::INFO,
		"TowerInfo: %zu definitions ready after runtime preload", definitions.size());

	runtimePreloaded = true;
}

void TowerInfoExt::FinalizeTowerRegistration(const Util::FlagManager& towerFlags)
{
	Print(LogLevel::INFO, "Hijacking tower info runtime to resolve custom tower unlocks/overviews...");
	PreloadRuntime();
	idToIndex.clear();
	for (size_t i = firstCustomDefinitionIndex; i < definitions.size(); ++i)
	{
		auto& def = definitions[i];
		if (IsTowerInfoExcludedName(def.towerType))
			continue;
		def.towerId = towerFlags.GetFlag(def.towerType);
		if (def.towerId != 0)
		{
			if (!IsCustomTowerInfoTower(def.towerId))
				continue;
			idToIndex[def.towerId] = i;
			Print(LogLevel::INFO, "TowerInfo: registered '%s' as tower ID %llu", def.towerType.c_str(), def.towerId);
		}
		else
		{
			Print(LogLevel::WARNING, "TowerInfo: tower '%s' has no registered tower ID yet", def.towerType.c_str());
		}
	}

	for (const auto& [towerId, towerName] : towerFlags.GetAll())
	{
		if (towerName.empty() || towerName == "INVALID")
			continue;
		if (IsTowerInfoExcludedName(towerName))
			continue;
		if (!IsCustomTowerInfoTower(towerId))
			continue;
		if (idToIndex.find(towerId) != idToIndex.end())
			continue;

		auto existing = nameToIndex.find(towerName);
		if (existing != nameToIndex.end())
		{
			definitions[existing->second].towerId = towerId;
			idToIndex[towerId] = existing->second;
			Print(LogLevel::INFO, "TowerInfo: registered '%s' as tower ID %llu", towerName.c_str(), towerId);
			continue;
		}

		TowerInfoDefinition def;
		def.towerType = towerName;
		def.towerId = towerId;
		def.canBeViewed = true;
		def.canBeViewedSpecified = true;
		const size_t idx = definitions.size();
		nameToIndex[towerName] = idx;
		idToIndex[towerId] = idx;
		definitions.push_back(std::move(def));
		Print(LogLevel::INFO, "TowerInfo: registered tower flag '%s' as tower ID %llu", towerName.c_str(), towerId);
	}

}


bool TowerInfoExt::BindDefinitionId(const std::string& towerType, uint64_t towerId)
{
	if (towerId == 0 || towerType.empty() || towerType == "INVALID")
		return false;
	if (IsTowerInfoExcludedName(towerType) || (!IsCustomTowerInfoTower(towerId) && !IsVanillaTowerInfoTower(towerId, towerType)))
		return false;

	const auto it = nameToIndex.find(towerType);
	if (it == nameToIndex.end())
		return false;

	auto& def = definitions[it->second];
	if (def.towerId != towerId)
	{
		def.towerId = towerId;
		Print(LogLevel::INFO, "TowerInfo: late-bound '%s' to tower ID %llu", towerType.c_str(), towerId);
	}
	idToIndex[towerId] = it->second;
	return true;
}

bool TowerInfoExt::ShouldDisplayInInfoPanel(const std::string& towerType, bool isCustomTower) const
{
	if (towerType.empty() || towerType == "INVALID")
	{
		return true;
	}
	if (IsTowerInfoExcludedName(towerType))
		return false;

	if (!loadedAny)
	{
		return true;
	}

	const TowerInfoDefinition* def = GetDefinition(towerType);
	if (def)
	{
		if (def->canBeViewedSpecified)
		{
			return def->canBeViewed;
		}

		if (def->towerId != 0)
			return IsCustomTowerInfoTower(def->towerId) || IsVanillaTowerInfoTower(def->towerId, towerType);

		return isCustomTower && !IsTowerInfoExcludedName(towerType);
	}

	return isCustomTower && !IsTowerInfoExcludedName(towerType);
}


bool TowerInfoExt::ShouldDisplayInInfoPanel(uint64_t towerId, const std::string& towerType, bool isCustomTower) const
{
	if (IsTowerInfoExcludedName(towerType))
		return false;
	if (!IsCustomTowerInfoTower(towerId) && !IsVanillaTowerInfoTower(towerId, towerType))
		return false;

	if (const TowerInfoDefinition* def = GetDefinition(towerId))
	{
		if (def->canBeViewedSpecified)
			return def->canBeViewed;
		return IsCustomTowerInfoTower(towerId) || IsVanillaTowerInfoTower(towerId, towerType);
	}

	if (!isCustomTower)
		return IsVanillaTowerInfoTower(towerId, towerType);

	return ShouldDisplayInInfoPanel(towerType, isCustomTower);
}

bool TowerInfoExt::IsVanillaTowerInfoTower(uint64_t towerId, const std::string& towerType)
{
	if (!Util::FlagManager::IsBitFlag(towerId) || IsTowerInfoExcludedName(towerType))
		return false;

	if ((towerId >= (1ull << 2ull) && towerId <= (1ull << 19ull)) ||
		towerId == (1ull << 22ull) ||
		towerId == (1ull << 23ull) ||
		towerId == (1ull << 24ull))
	{
		return true;
	}

	return false;
}

bool TowerInfoExt::IsCustomTowerInfoTower(uint64_t towerId)
{
	return Util::FlagManager::IsCustomBitFlag(towerId) || Util::FlagManager::IsCustomFallbackId(towerId);
}

bool TowerInfoExt::ShouldHideUpgradeUnlocks(const std::string& towerType) const
{
	return !CanUnlockTower(towerType);
}

bool TowerInfoExt::ShouldHideUpgradeUnlocks(uint64_t towerId, const std::string& towerType) const
{
	return !CanUnlockTower(towerId, towerType);
}

bool TowerInfoExt::CanUnlockTower(const std::string& towerType) const
{
	if (towerType.empty() || towerType == "INVALID")
		return true;

	if (!loadedAny)
		return true;

	const TowerInfoDefinition* def = GetDefinition(towerType);
	if (def)
		return def->canBeUnlocked;

	return true;
}

bool TowerInfoExt::CanUnlockTower(uint64_t towerId, const std::string& towerType) const
{
	if (const TowerInfoDefinition* def = GetDefinition(towerId))
		return def->canBeUnlocked;

	return CanUnlockTower(towerType);
}
