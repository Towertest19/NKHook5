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
                int labType;
                std::vector<int> levelGateway;
                std::vector<nlohmann::json> upgrades;
                int maxLevel;

                LabDefinition() : labType(-1), maxLevel(4) {} // Vanilla specialty buildings cap at tier IV (4)
        };

        class LabDefinitionsExt : public Common::Extensions::JsonExtension
        {
        private:
                std::vector<LabDefinition> definitions;
                std::map<std::string, size_t> nameToIndex;
                std::map<int, size_t> labTypeToIndex;

        public:
                LabDefinitionsExt();
                
                void UseJsonData(nlohmann::json content) override;
                void FinalizeTowerRegistration();
                
                const std::vector<LabDefinition>& GetDefinitions() const;
                const LabDefinition* GetDefinition(const std::string& name) const;
                const LabDefinition* GetDefinition(int labType) const;
                int GetMaxLevel(const std::string& labName) const;
                int GetMaxLevel(int labType) const;
                int GetFallbackMaxLevel(int vanillaMaxLevel) const;
        };
}
