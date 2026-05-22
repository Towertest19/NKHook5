#include "LabDefinitionsExt.h"

#include "../../Assets/AssetServer.h"
#include "../../Util/JetJsonLoader.h"

#include <Logging/Logger.h>

#include <cstring>
#include <algorithm>
#include <unordered_set>

using namespace Common;
using namespace Common::Extensions;
using namespace Common::Logging::Logger;
using namespace NKHook5;
using namespace NKHook5::Assets;
using namespace NKHook5::Extensions;
using namespace NKHook5::Extensions::LabDefinitions;
using namespace NKHook5::Util;

namespace
{
        static constexpr const char* kLabShopPath =
                "Assets/JSON/ScreenDefinitions/MainMenu/LabShop.json";

        static bool IsMonkeyLabVanillaCap(int vanillaMax)
        {
                return (vanillaMax >= 10 && vanillaMax <= 21) || vanillaMax == 13;
        }

        static std::string StripLabLocPrefix(const std::string& name)
        {
                static constexpr const char* kPrefix = "LOC_LAB_";
                if (name.rfind(kPrefix, 0) == 0 && name.size() > strlen(kPrefix))
                        return name.substr(strlen(kPrefix));
                return name;
        }
}

LabDefinitionsExt::LabDefinitionsExt() : JsonExtension("LabDefinitions", "*/Assets/JSON/LabDefinitions/*.json")
{
}

bool LabDefinitionsExt::ShouldSkipJson(const nlohmann::json& content)
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
        return false;
}

size_t LabDefinitionsExt::UpsertDefinition(LabDefinition def)
{
        auto it = nameToIndex.find(def.name);
        if (it != nameToIndex.end())
        {
                const size_t idx = it->second;
                if (definitions[idx].labType >= 0)
                        def.labType = definitions[idx].labType;
                definitions[idx] = std::move(def);
                return idx;
        }

        const size_t idx = definitions.size();
        nameToIndex[def.name] = idx;
        const std::string bare = StripLabLocPrefix(def.name);
        if (bare != def.name)
                nameToIndex[bare] = idx;
        if (def.labType >= 0)
                labTypeToIndex[def.labType] = idx;
        definitions.emplace_back(std::move(def));
        return idx;
}

void LabDefinitionsExt::UseJsonData(nlohmann::json content)
{
        if (content.empty() || ShouldSkipJson(content))
                return;

        if (content.is_array())
        {
                for (const auto& item : content)
                        UseJsonData(item);
                return;
        }

        if (!content.contains("Name"))
                return;

        try
        {
                LabDefinition def;
                def.name = content["Name"].get<std::string>();

                if (content.contains("Description"))
                        def.description = content["Description"].get<std::string>();
                if (content.contains("LevelDescription"))
                        def.levelDescription = content["LevelDescription"].get<std::string>();
                if (content.contains("Icon"))
                        def.icon = content["Icon"].get<std::string>();

                def.labType = content.value("LabType", -1);

                if (content.contains("LevelGateway"))
                        def.levelGateway = content["LevelGateway"].get<std::vector<int>>();

                if (content.contains("Upgrades"))
                {
                        def.upgrades = content["Upgrades"].get<std::vector<nlohmann::json>>();
                        def.maxLevel = static_cast<int>(def.upgrades.size());
                }
                else
                {
                        def.maxLevel = 13;
                }

                if (content.contains("MaxLevel"))
                        def.maxLevel = std::max(def.maxLevel, content["MaxLevel"].get<int>());

                const size_t idx = UpsertDefinition(std::move(def));
                Print(LogLevel::INFO,
                        "LabDefinitions: '%s' maxLevel=%d (labType=%d)",
                        definitions[idx].name.c_str(),
                        definitions[idx].maxLevel,
                        definitions[idx].labType);
        }
        catch (const std::exception& e)
        {
                Print(LogLevel::ERR, "LabDefinitions: parse failed: %s", e.what());
        }
}

void LabDefinitionsExt::PreloadRuntime()
{
        if (runtimePreloaded)
                return;

        Print(LogLevel::INFO, "Hijacking lab definitions runtime (post-tower) to preload merged .nkh/.jet data...");
        Print(LogLevel::INFO, "Copying vanilla lab definitions...");
        PreloadJsonExtension(*this);
        Print(LogLevel::INFO, "Old lab definitions copied; injecting mod overrides from merged assets.");

        LoadLabShopOrder();
        runtimePreloaded = true;

        Print(LogLevel::INFO,
                "LabDefinitions: %zu definition(s) ready after runtime preload",
                definitions.size());
}

void LabDefinitionsExt::LoadLabShopOrder()
{
        labShopOrder = ReadMergedShopTypes(kLabShopPath, "LabItems");
        labShopRecordIndex = 0;
        labTypeByShopType.clear();
        modLabTypesApplied = false;

        if (!labShopOrder.empty())
        {
                Print(LogLevel::INFO,
                        "LabDefinitions: LabShop order has %zu monkey-lab entries",
                        labShopOrder.size());
        }
}

void LabDefinitionsExt::RecordLabShopQuery(int labType, int vanillaMaxLevel)
{
        if (!IsMonkeyLabVanillaCap(vanillaMaxLevel))
                return;
        if (labShopRecordIndex >= labShopOrder.size())
                return;

        const std::string& shopType = labShopOrder[labShopRecordIndex];
        labTypeByShopType[shopType] = labType;
        ++labShopRecordIndex;

        Print(LogLevel::INFO,
                "LabDefinitions: LabShop[%zu] '%s' -> labType %d (vanilla cap %d)",
                labShopRecordIndex - 1, shopType.c_str(), labType, vanillaMaxLevel);

        if (labShopRecordIndex >= labShopOrder.size())
                ApplyModLabTypeBindings();
}

void LabDefinitionsExt::ApplyModLabTypeBindings()
{
	if (modLabTypesApplied)
		return;

	for (auto& def : definitions)
	{
		if (def.labType >= 0)
			continue;

		const std::string bare = StripLabLocPrefix(def.name);
		auto it = labTypeByShopType.find(bare);
		if (it == labTypeByShopType.end())
		{
			Print(LogLevel::INFO,
				"LabDefinitions: '%s' has no labType (no LabShop.json entry matches '%s') – "
				"labType remains -1",
				def.name.c_str(), bare.c_str());
			continue;
		}

		def.labType = it->second;
		labTypeToIndex[def.labType] = nameToIndex[def.name];

		Print(LogLevel::INFO,
			"LabDefinitions: bound '%s' -> labType %d (maxLevel %d)",
			def.name.c_str(), def.labType, def.maxLevel);
	}

	modLabTypesApplied = true;
}

const std::vector<LabDefinition>& LabDefinitionsExt::GetDefinitions() const
{
        return definitions;
}

const LabDefinition* LabDefinitionsExt::GetDefinition(const std::string& name) const
{
        auto it = nameToIndex.find(name);
        if (it != nameToIndex.end())
                return &definitions[it->second];
        return nullptr;
}

const LabDefinition* LabDefinitionsExt::GetDefinition(int labType) const
{
        auto it = labTypeToIndex.find(labType);
        if (it != labTypeToIndex.end())
                return &definitions[it->second];
        return nullptr;
}

int LabDefinitionsExt::GetMaxLevel(const std::string& labName) const
{
        const LabDefinition* def = GetDefinition(labName);
        if (def)
                return def->maxLevel;
        return -1;
}

int LabDefinitionsExt::GetMaxLevel(int labType) const
{
        const LabDefinition* def = GetDefinition(labType);
        if (def)
                return def->maxLevel;
        return -1;
}

int LabDefinitionsExt::GetFallbackMaxLevel(int vanillaMaxLevel, int labType) const
{
        if (!IsMonkeyLabVanillaCap(vanillaMaxLevel))
                return -1;

        const auto bound = labTypeToIndex.find(labType);
        if (bound != labTypeToIndex.end())
        {
                const int jsonMax = definitions[bound->second].maxLevel;
                return jsonMax > vanillaMaxLevel ? jsonMax : -1;
        }

        return -1;
}

bool LabDefinitionsExt::AugmentLabShopJson(nlohmann::json& root) const
{
        if (!root.contains("LabItems") || !root["LabItems"].is_array())
                root["LabItems"] = nlohmann::json::array();

        std::unordered_set<std::string> existing;
        for (const auto& item : root["LabItems"])
        {
                if (item.is_object() && item.contains("Type"))
                        existing.insert(item["Type"].get<std::string>());
        }

        AssetServer* server = AssetServer::GetServer();
        if (!server)
                return false;

        bool changed = false;
        for (const std::string& path : server->CollectEntryPaths("Assets/JSON/LabDefinitions/", ".json"))
        {
                if (path.find("CacheList") != std::string::npos)
                        continue;

                const size_t slash = path.find_last_of('/');
                std::string shopType = slash == std::string::npos ? path : path.substr(slash + 1);
                const size_t dot = shopType.find_last_of('.');
                if (dot != std::string::npos)
                        shopType = shopType.substr(0, dot);

                if (shopType.empty() || existing.contains(shopType))
                        continue;

                root["LabItems"].push_back({ { "Type", shopType } });
                existing.insert(shopType);
                changed = true;
                Print(LogLevel::INFO,
                        "LabDefinitions: added '%s' to LabShop.json",
                        shopType.c_str());
        }

        return changed;
}
