#include "LabDefinitionsExt.h"

#include <Logging/Logger.h>

#include <algorithm>
#include <cstring>

using namespace Common;
using namespace Common::Extensions;
using namespace Common::Logging::Logger;
using namespace NKHook5;
using namespace NKHook5::Extensions;
using namespace NKHook5::Extensions::LabDefinitions;

namespace
{
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
                if (def.labType < 0 && definitions[idx].labType >= 0)
                        def.labType = definitions[idx].labType;
                definitions[idx] = std::move(def);
                if (definitions[idx].labType >= 0)
                        labTypeToIndex[definitions[idx].labType] = idx;
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

        Print(LogLevel::INFO, "LabDefinitions: priming runtime state without scanning Assets/JSON/LabDefinitions.");

        runtimePreloaded = true;

        Print(LogLevel::INFO,
                "LabDefinitions: %zu definition(s) ready after runtime preload",
                definitions.size());
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
