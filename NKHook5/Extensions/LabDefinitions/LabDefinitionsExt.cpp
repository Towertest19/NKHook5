#include "LabDefinitionsExt.h"

#include <Logging/Logger.h>

using namespace Common;
using namespace Common::Extensions;
using namespace Common::Logging::Logger;
using namespace NKHook5;
using namespace NKHook5::Extensions;
using namespace NKHook5::Extensions::LabDefinitions;

LabDefinitionsExt::LabDefinitionsExt() : JsonExtension("LabDefinitions", "Assets/JSON/LabDefinitions/*.json")
{
}

void LabDefinitionsExt::UseJsonData(nlohmann::json content)
{
        if (content.empty())
        {
                Print(LogLevel::ERR, "Received empty json data for a LabDefinition");
                return;
        }
        
        if (!content.contains("Name"))
        {
                Print(LogLevel::ERR, "Received a LabDefinition without a 'Name' property!");
                return;
        }

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
                        
                if (content.contains("LevelGateway"))
                        def.levelGateway = content["LevelGateway"].get<std::vector<int>>();
                        
                if (content.contains("Upgrades"))
                {
                        def.upgrades = content["Upgrades"].get<std::vector<nlohmann::json>>();
                        // Calculate max level from actual upgrades array size
                        def.maxLevel = static_cast<int>(def.upgrades.size());
                        
                        Print(LogLevel::INFO, "Lab '%s': calculated max level = %d (from %zu upgrades)", 
                                def.name.c_str(), def.maxLevel, def.upgrades.size());
                }
                else
                {
                        // No upgrades array — vanilla specialty buildings cap at tier IV (4).
                        def.maxLevel = 4;
                        Print(LogLevel::WARNING, "Lab '%s': no upgrades found, using vanilla default max level = %d",
                                def.name.c_str(), def.maxLevel);
                }

                // Store in map for quick lookup
                nameToIndex[def.name] = definitions.size();
                {
                        static constexpr const char* kPrefix = "LOC_LAB_";
                        if (def.name.rfind(kPrefix, 0) == 0 && def.name.size() > strlen(kPrefix))
                        {
                                nameToIndex[def.name.substr(strlen(kPrefix))] = definitions.size();
                        }
                }
                definitions.emplace_back(std::move(def));
                
                Print(LogLevel::INFO, "Loaded LabDefinition: '%s' with max level %d", 
                        def.name.c_str(), def.maxLevel);
        }
        catch (const std::exception& e)
        {
                Print(LogLevel::ERR, "Failed to parse LabDefinition JSON: %s", e.what());
        }
}

const std::vector<LabDefinition>& LabDefinitionsExt::GetDefinitions() const
{
        return this->definitions;
}

const LabDefinition* LabDefinitionsExt::GetDefinition(const std::string& name) const
{
        auto it = nameToIndex.find(name);
        if (it != nameToIndex.end())
        {
                return &definitions[it->second];
        }
        return nullptr;
}

int LabDefinitionsExt::GetMaxLevel(const std::string& labName) const
{
        const LabDefinition* def = GetDefinition(labName);
        if (def)
        {
                return def->maxLevel;
        }
        
        // Vanilla specialty buildings cap at tier IV (4).
        Print(LogLevel::WARNING, "Lab '%s' not found, using vanilla default max level = 4", labName.c_str());
        return 4;
}
