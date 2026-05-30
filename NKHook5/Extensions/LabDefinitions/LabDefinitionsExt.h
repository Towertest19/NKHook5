#pragma once

#include <Extensions/JsonExtension.h>
#include <vector>
#include <string>
#include <cstddef>
#include <unordered_map>
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

                LabDefinition() : labType(-1), maxLevel(4) {}
        };

        class LabDefinitionsExt : public Common::Extensions::JsonExtension
        {
        private:
                std::vector<LabDefinition> definitions;
                std::unordered_map<std::string, size_t> nameToIndex;
                std::unordered_map<int, size_t> labTypeToIndex;
                bool runtimePreloaded = false;

                static bool ShouldSkipJson(const nlohmann::json& content);
                size_t UpsertDefinition(LabDefinition def);

        public:
                LabDefinitionsExt();
                
                void UseJsonData(nlohmann::json content) override;
                void PreloadRuntime();
                bool IsRuntimePreloaded() const { return runtimePreloaded; }
                
                const std::vector<LabDefinition>& GetDefinitions() const;
                const LabDefinition* GetDefinition(const std::string& name) const;
                const LabDefinition* GetDefinition(int labType) const;
                int GetMaxLevel(const std::string& labName) const;
                int GetMaxLevel(int labType) const;
                int GetFallbackMaxLevel(int vanillaMaxLevel, int labType) const;
                int GetHighestDefinedMaxLevel() const;

        };
}
