#include "SpecialtyDefinitionsExt.h"

#include "../../Assets/AssetServer.h"
#include "../../Util/JetJsonLoader.h"

#include <Logging/Logger.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_set>

using namespace Common;
using namespace Common::Extensions;
using namespace Common::Logging::Logger;
using namespace NKHook5;
using namespace NKHook5::Assets;
using namespace NKHook5::Extensions;
using namespace NKHook5::Extensions::SpecialtyDefinitions;
using namespace NKHook5::Util;

namespace fs = std::filesystem;

namespace
{
        static constexpr std::array<std::pair<const char*, int>, 9> kTierOrder = {{
                { "I",    1 },
                { "II",   2 },
                { "III",  3 },
                { "IV",   4 },
                { "V",    5 },
                { "VI",   6 },
                { "VII",  7 },
                { "VIII", 8 },
                { "IX",   9 },
        }};

        static constexpr const char* kSpecialtyShopPath =
                "Assets/JSON/ScreenDefinitions/MainMenu/SpecialtyShop.json";

        static bool IsSpecialtyVanillaCap(int vanillaMax)
        {
                return vanillaMax == kVanillaMaxLevel;
        }

        static std::string NormalizeShopFile(const std::string& type)
        {
                if (type.ends_with(".json"))
                        return type;
                return type + ".json";
        }

        static std::string StripJsonExtension(std::string fileName)
        {
                if (fileName.ends_with(".json"))
                        fileName.resize(fileName.size() - 5);
                return fileName;
        }

        static std::string DefinitionShopKey(const SpecialtyDefinition& def)
        {
                return def.fileName.empty() ? def.name : def.fileName;
        }

        static std::string Lowercase(std::string value)
        {
                std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                        return static_cast<char>(std::tolower(ch));
                });
                return value;
        }

        static void AddShopAliases(std::unordered_map<std::string, std::string>& aliases,
                const SpecialtyDefinition& def)
        {
                const std::string shopFile = NormalizeShopFile(DefinitionShopKey(def));
                if (shopFile.empty())
                        return;
                aliases[Lowercase(NormalizeShopFile(def.name))] = shopFile;
                aliases[Lowercase(shopFile)] = shopFile;
        }

        static int RomanTierValue(const std::string& key)
        {
                for (const auto& [tierKey, tierIdx] : kTierOrder)
                {
                        if (key == tierKey)
                                return tierIdx;
                }
                if (key == "X")
                        return 10;
                return 0;
        }
}

SpecialtyDefinitionsExt::SpecialtyDefinitionsExt()
        : JsonExtension("SpecialtyDefinitions", "*/Assets/JSON/SpecialtyDefinitions/*.json")
{
}

bool SpecialtyDefinitionsExt::ShouldSkipJson(const nlohmann::json& content)
{
        if (content.contains("FileName"))
        {
                const auto fileName = content["FileName"].get<std::string>();
                if (fileName.find("CacheList") != std::string::npos)
                        return true;
        }
        if (content.contains("Name"))
        {
                const auto name = content["Name"].get<std::string>();
                if (name.find("CacheList") != std::string::npos)
                        return true;
        }
        if (!content.contains("Name"))
                return true;
        return false;
}

size_t SpecialtyDefinitionsExt::UpsertDefinition(SpecialtyDefinition def)
{
        auto it = nameToIndex.find(def.name);
        if (it == nameToIndex.end() && !def.fileName.empty())
                it = nameToIndex.find(def.fileName);
        if (it != nameToIndex.end())
        {
                const size_t idx = it->second;
                if (definitions[idx].labType >= 0)
                        def.labType = definitions[idx].labType;
                if (def.fileName.empty())
                        def.fileName = definitions[idx].fileName;
                AddShopAliases(specialtyShopTypeAliases, def);
                definitions[idx] = std::move(def);
                return idx;
        }

        const size_t idx = definitions.size();
        nameToIndex[def.name] = idx;
        if (!def.fileName.empty())
                nameToIndex[def.fileName] = idx;
        AddShopAliases(specialtyShopTypeAliases, def);

        static constexpr const char* kPrefix = "LOC_SPEC_";
        static constexpr size_t kPrefixLen = 9;
        if (def.name.rfind(kPrefix, 0) == 0 && def.name.size() > kPrefixLen)
                nameToIndex[def.name.substr(kPrefixLen)] = idx;

        if (def.labType >= 0)
                labTypeToIndex[def.labType] = idx;

        definitions.emplace_back(std::move(def));
        return idx;
}

int SpecialtyDefinitionsExt::CountTiers(const nlohmann::json& effects, bool clampToRuntimeMax)
{
        if (!effects.is_object())
                return 0;

        int highest = 0;
        for (const auto& item : effects.items())
        {
                const int tierIdx = RomanTierValue(item.key());
                if (tierIdx > highest)
                        highest = tierIdx;
        }
        if (clampToRuntimeMax)
                return std::min(highest, 9);
        return highest;
}

void SpecialtyDefinitionsExt::UseJsonData(nlohmann::json content)
{
        if (content.empty() || ShouldSkipJson(content))
                return;

        if (content.is_array())
        {
                for (const auto& item : content)
                        UseJsonData(item);
                return;
        }

        try
        {
                SpecialtyDefinition def;
                def.name = content["Name"].get<std::string>();
                if (content.contains("FileName"))
                        def.fileName = StripJsonExtension(content["FileName"].get<std::string>());

                if (content.contains("Building"))
                        def.building = content["Building"].get<std::string>();

                def.labType = content.value("LabType", -1);

                if (content.contains("Effects") && content["Effects"].is_object())
                {
                        const auto& effects = content["Effects"];
                        def.maxLevel = CountTiers(effects);

                        for (const auto& [key, tierIdx] : kTierOrder)
                        {
                                if (effects.contains(key))
                                        def.tiers.emplace_back(key);
                        }

                        if (def.maxLevel == 0)
                                def.maxLevel = kVanillaMaxLevel;
                }
                else
                {
                        def.maxLevel = content.value("MaxLevel", kVanillaMaxLevel);
                }

                const size_t idx = UpsertDefinition(std::move(def));

                std::string tierSummary;
                for (size_t i = 0; i < definitions[idx].tiers.size(); ++i)
                {
                        if (i) tierSummary += ',';
                        tierSummary += definitions[idx].tiers[i];
                }
                if (tierSummary.empty()) tierSummary = "(none)";

                Print(LogLevel::INFO,
                        "SpecialtyDefinitions: '%s' maxLevel=%d tiers=[%s] (labType=%d)",
                        definitions[idx].name.c_str(),
                        definitions[idx].maxLevel,
                        tierSummary.c_str(),
                        definitions[idx].labType);
        }
        catch (const std::exception& e)
        {
                Print(LogLevel::ERR, "SpecialtyDefinitions: parse failed: %s", e.what());
        }
}

void SpecialtyDefinitionsExt::PreloadRuntime()
{
        if (runtimePreloaded)
                return;

        Print(LogLevel::INFO, "SpecialtyDefinitions: priming runtime state without scanning Assets/JSON/SpecialtyDefinitions.");

        LoadSpecialtyShopOrder();

        std::unordered_set<std::string> existingShopTypes;
        for (std::string& shopType : specialtyShopOrder)
        {
                const auto alias = specialtyShopTypeAliases.find(Lowercase(NormalizeShopFile(shopType)));
                if (alias != specialtyShopTypeAliases.end())
                        shopType = alias->second;
                existingShopTypes.insert(Lowercase(NormalizeShopFile(shopType)));
        }
        for (const SpecialtyDefinition& def : definitions)
        {
                const std::string shopFile = NormalizeShopFile(DefinitionShopKey(def));
                if (shopFile.empty() || !existingShopTypes.insert(Lowercase(shopFile)).second)
                        continue;
                specialtyShopOrder.push_back(shopFile);
        }

        runtimePreloaded = true;

        Print(LogLevel::INFO,
                "SpecialtyDefinitions: %zu definition(s) ready after runtime preload",
                definitions.size());
}

void SpecialtyDefinitionsExt::LoadSpecialtyShopOrder()
{
        specialtyShopOrder = ReadMergedShopTypes(kSpecialtyShopPath, "SpecialtyItems");
        specialtyRecordIndex = 0;
        buildingToLabType.clear();
        specialtyShopTypeAliases.clear();
        modSpecialtyTypesApplied = false;

        if (!specialtyShopOrder.empty())
        {
                Print(LogLevel::INFO,
                        "SpecialtyDefinitions: SpecialtyShop order has %zu entries",
                        specialtyShopOrder.size());
        }
}

void SpecialtyDefinitionsExt::RecordSpecialtyShopQuery(int labType, int vanillaMaxLevel)
{
	if (!IsSpecialtyVanillaCap(vanillaMaxLevel))
		return;
	if (specialtyRecordIndex >= specialtyShopOrder.size())
		return;

	const std::string shopFile = NormalizeShopFile(specialtyShopOrder[specialtyRecordIndex]);
	const std::string entryPath = "Assets/JSON/SpecialtyDefinitions/" + shopFile;

	nlohmann::json vanillaDef;
	std::string building;
	if (ReadMergedJsonEntry(entryPath, vanillaDef) && vanillaDef.contains("Building"))
		building = vanillaDef["Building"].get<std::string>();

	buildingToLabType[shopFile] = labType;

	// Fallback: if the vanilla mod file has no 'Building' field (common when
	// mods do not include SpecialtyShop.json overrides), try reading the
	// mod-merged JSON for that path directly so we can extract the name.
	if (building.empty())
	{
		std::vector<uint8_t> raw;
		if (AssetServer* server = AssetServer::GetServer())
		{
			if (auto served = server->Serve(fs::path(entryPath), std::vector<uint8_t>{}))
				raw = served->GetData();
		}
		if (raw.empty())
		{
			// Try vanilla jet as last resort.
			ReadVanillaJetBytes(entryPath, raw);
		}

		if (!raw.empty())
		{
			try
			{
				auto parsed = nlohmann::json::parse(
					std::string(reinterpret_cast<const char*>(raw.data()), raw.size()),
					nullptr, true, true);
				if (parsed.is_object() && parsed.contains("Building"))
					building = parsed["Building"].get<std::string>();
			}
			catch (const std::exception&)
			{
			}
		}
	}

	if (!building.empty())
		buildingToLabType[building] = labType;

	++specialtyRecordIndex;

	Print(LogLevel::INFO,
		"SpecialtyDefinitions: SpecialtyShop '%s' building='%s' -> labType %d",
		shopFile.c_str(), building.c_str(), labType);

	if (specialtyRecordIndex >= specialtyShopOrder.size())
		ApplyModSpecialtyBindings();
}

void SpecialtyDefinitionsExt::ApplyModSpecialtyBindings()
{
        if (modSpecialtyTypesApplied)
                return;

        for (auto& def : definitions)
        {
                if (def.labType >= 0)
                        continue;
                if (def.building.empty())
                        continue;

                auto it = buildingToLabType.find(def.building);
                if (it == buildingToLabType.end())
                        it = buildingToLabType.find(NormalizeShopFile(DefinitionShopKey(def)));
                if (it == buildingToLabType.end())
                        continue;

                def.labType = it->second;
                labTypeToIndex[def.labType] = nameToIndex[def.name];

                Print(LogLevel::INFO,
                        "SpecialtyDefinitions: bound '%s' (building='%s') -> labType %d (maxLevel %d)",
                        def.name.c_str(), def.building.c_str(), def.labType, def.maxLevel);
        }

        modSpecialtyTypesApplied = true;
}

int SpecialtyDefinitionsExt::GetMaxLevel(int labType) const
{
        const auto it = labTypeToIndex.find(labType);
        if (it != labTypeToIndex.end())
                return definitions[it->second].maxLevel;
        return -1;
}

int SpecialtyDefinitionsExt::GetFallbackMaxLevel(int vanillaMaxLevel, int labType) const
{
        if (!IsSpecialtyVanillaCap(vanillaMaxLevel))
                return -1;

        const auto bound = labTypeToIndex.find(labType);
        if (bound != labTypeToIndex.end())
        {
                const int jsonMax = definitions[bound->second].maxLevel;
                return jsonMax > vanillaMaxLevel ? jsonMax : -1;
        }

        return -1;
}

int SpecialtyDefinitionsExt::GetMaxLevel(const std::string& name) const
{
        const auto it = nameToIndex.find(name);
        if (it != nameToIndex.end())
                return definitions[it->second].maxLevel;
        return -1;
}

const SpecialtyDefinition* SpecialtyDefinitionsExt::GetDefinition(int labType) const
{
        const auto it = labTypeToIndex.find(labType);
        if (it != labTypeToIndex.end())
                return &definitions[it->second];
        return nullptr;
}

const SpecialtyDefinition* SpecialtyDefinitionsExt::GetDefinition(const std::string& name) const
{
        const auto it = nameToIndex.find(name);
        if (it != nameToIndex.end())
                return &definitions[it->second];
        return nullptr;
}

const std::vector<SpecialtyDefinition>& SpecialtyDefinitionsExt::GetDefinitions() const
{
        return definitions;
}

bool SpecialtyDefinitionsExt::AugmentSpecialtyShopJson(nlohmann::json& root) const
{
        if (!root.contains("SpecialtyItems") || !root["SpecialtyItems"].is_array())
                root["SpecialtyItems"] = nlohmann::json::array();

        for (auto& item : root["SpecialtyItems"])
        {
                if (!item.is_object() || !item.contains("Type"))
                        continue;
                const std::string type = item["Type"].get<std::string>();
                const auto alias = specialtyShopTypeAliases.find(Lowercase(NormalizeShopFile(type)));
                if (alias != specialtyShopTypeAliases.end())
                        item["Type"] = alias->second;
        }

        std::unordered_set<std::string> existing;
        for (const auto& item : root["SpecialtyItems"])
        {
                if (!item.is_object() || !item.contains("Type"))
                        continue;
                existing.insert(Lowercase(NormalizeShopFile(item["Type"].get<std::string>())));
        }

        bool changed = false;
        for (const SpecialtyDefinition& def : definitions)
        {
                std::string shopFile = DefinitionShopKey(def);
                if (shopFile.empty() || existing.contains(Lowercase(NormalizeShopFile(shopFile))))
                        continue;

                shopFile = NormalizeShopFile(shopFile);

                root["SpecialtyItems"].push_back({
                        { "Type", shopFile },
                        { "Offset", nlohmann::json::array({ 0, -5 }) }
                });
                existing.insert(Lowercase(shopFile));
                changed = true;
                Print(LogLevel::INFO,
                        "SpecialtyDefinitions: added '%s' to SpecialtyShop.json",
                        shopFile.c_str());
        }

        return changed;
}
