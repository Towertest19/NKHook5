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

		if (json.contains("HideUpgradeUnlocks") && json.value("HideUpgradeUnlocks", false))
			def.hideUpgradeUnlocks = true;

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

		Print(LogLevel::DEBUG, "Loaded TowerDefinition: '%s' (CanBeViewed=%s%s, CanBeUnlocked=%s)",
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

	Print(LogLevel::DEBUG, "Hijacking tower info runtime to preload merged .nkh/.jet metadata...");
	Print(LogLevel::DEBUG, "Copying vanilla tower info definitions...");
	PreloadJsonExtension(*this);
	firstCustomDefinitionIndex = definitions.size();
	Print(LogLevel::DEBUG, "Old tower info definitions copied; mod tower metadata will bind from tower flags.");

	firstCustomDefinitionIndex = std::min(firstCustomDefinitionIndex, definitions.size());
	Print(LogLevel::DEBUG,
		"TowerInfo: %zu definitions ready after runtime preload", definitions.size());

	runtimePreloaded = true;
}

void TowerInfoExt::FinalizeTowerRegistration(const Util::FlagManager& towerFlags)
{
	PreloadRuntime();
	const auto& allTowerFlags = towerFlags.GetAll();
	const size_t currentFlagCount = allTowerFlags.size();
	if (registrationFinalized && finalizedFlagCount == currentFlagCount && finalizedDefinitionCount == definitions.size())
		return;
	Print(LogLevel::DEBUG, "Hijacking tower info runtime to resolve custom tower unlocks/overviews...");
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
		}
	}

	for (const auto& [towerId, towerName] : allTowerFlags)
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
	}
	registrationFinalized = true;
	finalizedFlagCount = currentFlagCount;
	finalizedDefinitionCount = definitions.size();

}



std::vector<TowerInfoDefinition> TowerInfoExt::GetRuntimeOrderedDefinitions() const
{
	std::vector<TowerInfoDefinition> ordered;
	std::unordered_set<std::string> seen;

	static constexpr uint64_t kVanillaOrder[] = {
		1ull << 2ull,
		1ull << 23ull,
		1ull << 24ull,
		1ull << 22ull,
		1ull << 3ull,
		1ull << 4ull,
		1ull << 5ull,
		1ull << 6ull,
		1ull << 7ull,
		1ull << 8ull,
		1ull << 9ull,
		1ull << 10ull,
		1ull << 11ull,
		1ull << 12ull,
		1ull << 13ull,
		1ull << 14ull,
		1ull << 15ull,
		1ull << 16ull,
		1ull << 17ull,
		1ull << 18ull,
		1ull << 19ull,
	};

	for (const uint64_t towerId : kVanillaOrder)
	{
		if (const TowerInfoDefinition* def = GetDefinition(towerId))
		{
			if (ShouldDisplayInInfoPanel(def->towerId, def->towerType, false) && seen.insert(def->towerType).second)
				ordered.push_back(*def);
		}
	}

	for (const auto& def : definitions)
	{
		if (def.towerId == 0 || seen.contains(def.towerType))
			continue;
		if (ShouldDisplayInInfoPanel(def.towerId, def.towerType, IsCustomTowerInfoTower(def.towerId)))
		{
			seen.insert(def.towerType);
			ordered.push_back(def);
		}
	}

	return ordered;
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
	return Util::FlagManager::IsCustomTowerRuntimeId(towerId);
}

bool TowerInfoExt::ShouldHideUpgradeUnlocks(const std::string& towerType) const
{
	if (const TowerInfoDefinition* def = GetDefinition(towerType))
		return def->hideUpgradeUnlocks;
	return false;
}

bool TowerInfoExt::ShouldHideUpgradeUnlocks(uint64_t towerId, const std::string& towerType) const
{
	if (const TowerInfoDefinition* def = GetDefinition(towerId))
		return def->hideUpgradeUnlocks;
	return ShouldHideUpgradeUnlocks(towerType);
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
