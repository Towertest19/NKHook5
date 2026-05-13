#pragma once

#include <Extensions/JsonExtension.h>
#include <vector>
#include <string>
#include <map>
#include <nlohmann/json.hpp>

namespace NKHook5::Extensions::LabDefinitions
{
        struct LabDefinition
        {
                std::string name;
                std::string description;
                std::string levelDescription;
                std::string icon;
                std::vector<int> levelGateway;
                std::vector<nlohmann::json> upgrades;
                int maxLevel;

                LabDefinition() : maxLevel(4) {} // Vanilla specialty buildings cap at tier IV (4)
        };

        class LabDefinitionsExt : public Common::Extensions::JsonExtension
        {
        private:
                std::vector<LabDefinition> definitions;
                std::map<std::string, size_t> nameToIndex;

        public:
                LabDefinitionsExt();
                
                void UseJsonData(nlohmann::json content) override;
                
                const std::vector<LabDefinition>& GetDefinitions() const;
                const LabDefinition* GetDefinition(const std::string& name) const;
                int GetMaxLevel(const std::string& labName) const;
        };
}
