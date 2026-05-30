#include "SpecialtyDefinitionsExt.h"

#include <Logging/Logger.h>

#include <algorithm>
#include <array>

using namespace Common;
using namespace Common::Extensions;
using namespace Common::Logging::Logger;
using namespace NKHook5;
using namespace NKHook5::Extensions;
using namespace NKHook5::Extensions::SpecialtyDefinitions;

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

        static bool IsSpecialtyVanillaCap(int vanillaMax)
        {
                return vanillaMax == kVanillaMaxLevel;
        }

        static std::string StripJsonExtension(std::string fileName)
        {
                if (fileName.ends_with(".json"))
                        fileName.resize(fileName.size() - 5);
                return fileName;
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
                if (def.labType < 0 && definitions[idx].labType >= 0)
                        def.labType = definitions[idx].labType;
                if (def.fileName.empty())
                        def.fileName = definitions[idx].fileName;
                definitions[idx] = std::move(def);
                if (definitions[idx].labType >= 0)
                        labTypeToIndex[definitions[idx].labType] = idx;
                return idx;
        }

        const size_t idx = definitions.size();
        nameToIndex[def.name] = idx;
        if (!def.fileName.empty())
                nameToIndex[def.fileName] = idx;

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

        runtimePreloaded = true;

        Print(LogLevel::INFO,
                "SpecialtyDefinitions: %zu definition(s) ready after runtime preload",
                definitions.size());
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
