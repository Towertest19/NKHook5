#include "TowerInfoExt.h"

#include "../../Assets/AssetServer.h"
#include "../../Util/FlagManager.h"
#include "../../Util/JetJsonLoader.h"

#include <cstring>

#include <Extensions/ExtensionManager.h>
#include <Logging/Logger.h>

#include <unordered_set>

using namespace Common;
using namespace Common::Extensions;
using namespace Common::Logging::Logger;
using namespace NKHook5;
using namespace NKHook5::Assets;
using namespace NKHook5::Extensions;
using namespace NKHook5::Extensions::TowerInfo;
using namespace NKHook5::Util;

namespace fs = std::filesystem;

namespace
{
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
	Print(LogLevel::INFO, "Old tower info definitions copied; mod tower metadata is now available.");

	// Extra pass: collect any TypeNames that arrived via TowerFlags / Lua
	// after our own PreloadJsonExtension ran.  Flags.json or NKHookAutoload.lua
	// may register new tower bit-flags _after_ PreloadJsonExtension but before
	// this runtime hook fires, so idToIndex would still be empty for them.
	if (AssetServer* server = AssetServer::GetServer())
	{
		for (const auto& path : server->CollectEntryPaths("Assets/JSON/TowerDefinitions/", ".tower"))
		{
			nlohmann::json merged;
			if (!ReadMergedJsonEntry(path, merged))
				continue;
			if (!merged.is_object() || !merged.contains("TypeName"))
				continue;
			const std::string typeName = merged.value("TypeName", "");
			if (typeName.empty())
				continue;
			// Already present – skip.
			if (nameToIndex.find(typeName) != nameToIndex.end())
				continue;
			// Temporarily register so PreloadJsonExtension's idToIndex matches are complete
			// before FinalizeTowerRegistration walks this list.
			TowerInfoDefinition def;
			def.towerType = typeName;
			ReadTowerInfoToggles(merged, def);
			nameToIndex[typeName] = definitions.size();
			definitions.push_back(std::move(def));
			Print(LogLevel::INFO,
				"TowerInfo preload: discovered TypeName '%s' in '%s', id deferred to FinalizeTowerRegistration",
				typeName.c_str(), path.c_str());
		}
	}

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

	// Scan the merged asset-server catalogue for any additional .tower TypeNames
	// that were introduced by .nkh mods after this extension's own PreloadRuntime.
	// Those TypeNames may not yet have their idToIndex entry because their tower-id
	// bit flags were only just registered by Flags.json / the Lua extension.
	if (AssetServer* server = AssetServer::GetServer())
	{
		for (const auto& path : server->CollectEntryPaths("Assets/JSON/TowerDefinitions/", ".tower"))
		{
			nlohmann::json merged;
			if (!ReadMergedJsonEntry(path, merged))
				continue;
			if (!merged.is_object() || !merged.contains("TypeName"))
				continue;
			std::string typeName = merged.value("TypeName", "");
			if (typeName.empty())
				continue;
			// Only bind TypeNames we haven't seen yet.
			if (nameToIndex.find(typeName) != nameToIndex.end())
				continue;
			uint64_t flagId = towerFlags.GetFlag(typeName);
			if (flagId == 0)
			{
				Print(LogLevel::WARNING,
					"TowerInfo: TypeName '%s' found in '%s' has no tower flag – "
					"ensure it is listed in Flags.json",
					typeName.c_str(), path.c_str());
				continue;
			}
			// Clone the vanilla entry and register it as a dynamic tower definition.
			TowerInfoDefinition def;
			def.towerType = typeName;
			def.towerId = flagId;
			ReadTowerInfoToggles(merged, def);
			const size_t idx = definitions.size();
			nameToIndex[typeName] = idx;
			idToIndex[flagId] = idx;
			definitions.push_back(std::move(def));
			Print(LogLevel::INFO,
				"TowerInfo: late-bound TypeName '%s' from '%s' → tower ID %llu",
				typeName.c_str(), path.c_str(), flagId);
		}
	}
}


bool TowerInfoExt::BindDefinitionId(const std::string& towerType, uint64_t towerId)
{
	if (towerId == 0 || towerType.empty() || towerType == "INVALID")
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

		return true;
	}

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

bool TowerInfoExt::AugmentBuildingsJson(nlohmann::json& root, const Util::FlagManager& towerFlags)
{
	if (!root.contains("Buildings") || !root["Buildings"].is_array())
		return false;

	TowerInfoExt* towerInfoExt = ExtensionManager::Get<TowerInfoExt>();
	bool changed = false;

	for (auto& building : root["Buildings"])
	{
		if (!building.is_object() || building.value("Screen", "") != "TowerInfoScreen")
			continue;
		if (!building.contains("SubItems") || !building["SubItems"].is_array())
			building["SubItems"] = nlohmann::json::array();

		auto& subItems = building["SubItems"];
		std::unordered_set<std::string> existing;
		for (const auto& item : subItems)
		{
			if (item.is_object() && item.contains("Tower"))
				existing.insert(item["Tower"].get<std::string>());
		}

		for (const auto& [towerId, towerName] : towerFlags.GetAll())
		{
			if (towerName.empty() || towerName == "INVALID")
				continue;
			if (Util::FlagManager::IsBaseTower(towerId))
				continue;
			if (!Util::FlagManager::IsCustomBitFlag(towerId) && !Util::FlagManager::IsCustomFallbackId(towerId))
				continue;
			if (!existing.insert(towerName).second)
				continue;

			bool canView = true;
			if (towerInfoExt)
				canView = towerInfoExt->ShouldDisplayInInfoPanel(towerName, true);

			if (!canView)
				continue;

			subItems.push_back({
				{ "Type", "Present" },
				{ "Tower", towerName },
				{ "Position", nlohmann::json::array({ 0, 0 }) },
				{ "ButtonOffset", nlohmann::json::array({ 0, 0 }) },
				{ "Radius", 50 }
			});
			changed = true;
			Print(LogLevel::INFO, "TowerInfo: added '%s' to TowerInfoScreen Buildings.json", towerName.c_str());
		}
	}

	return changed;
}

bool TowerInfoExt::TryAugmentBuildingsBytes(std::vector<uint8_t>& data, const Util::FlagManager& towerFlags)
{
	if (data.empty() || towerFlags.GetAll().empty())
		return false;

	try
	{
		nlohmann::json buildings = nlohmann::json::parse(
			std::string(reinterpret_cast<const char*>(data.data()), data.size()),
			nullptr, true, true);
		if (!AugmentBuildingsJson(buildings, towerFlags))
			return false;

		const std::string patched = buildings.dump();
		data.assign(patched.begin(), patched.end());
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

void TowerInfoExt::RefreshBuildingsScreenEntries(const Util::FlagManager& towerFlags)
{
	if (towerFlags.GetAll().empty())
	{
		Print(LogLevel::WARNING,
			"TowerInfo: cannot refresh Buildings.json — tower flags not registered yet");
		return;
	}

	static constexpr const char* kBuildingPaths[] = {
		"Assets/JSON/ScreenDefinitions/MainMenu/Buildings.json",
		"Assets/JSON/ScreenDefinitions/MainMenu/BuildingsNoSocial.json",
	};

	AssetServer* server = AssetServer::GetServer();
	if (!server)
		return;

	for (const char* entryPath : kBuildingPaths)
	{
		nlohmann::json merged;
		if (!ReadMergedJsonEntry(entryPath, merged))
			continue;

		std::vector<uint8_t> mergedVec;
		{
			const std::string serialized = merged.dump();
			mergedVec.assign(serialized.begin(), serialized.end());
		}
		if (!TryAugmentBuildingsBytes(mergedVec, towerFlags))
			continue;

		server->InvalidateCachedPath(entryPath);
		server->CacheServedAsset(entryPath, std::make_shared<Asset>(fs::path(entryPath), mergedVec));
		Print(LogLevel::INFO,
			"TowerInfo: refreshed merged '%s' with custom tower SubItems", entryPath);
	}
}
