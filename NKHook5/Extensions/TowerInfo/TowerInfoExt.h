#pragma once

#include <Extensions/JsonExtension.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace NKHook5::Util
{
	class FlagManager;
}

namespace NKHook5
{
	namespace Extensions
	{
		namespace TowerInfo
		{
			using namespace Common;
			using namespace Common::Extensions;

			struct TowerInfoDefinition
			{
				std::string towerType;
				uint64_t towerId = 0;
				bool canBeViewedSpecified = false;
				bool canBeViewed = true;
				bool canBeUnlocked = true;
				std::string customDescription;  // Optional custom description
			};

			class TowerInfoExt : public JsonExtension
			{
				std::vector<TowerInfoDefinition> definitions;
				std::unordered_map<std::string, size_t> nameToIndex;
				std::unordered_map<uint64_t, size_t> idToIndex;
				bool loadedAny = false;
				bool runtimePreloaded = false;
				size_t firstCustomDefinitionIndex = 0;
			public:
				TowerInfoExt();
				virtual const std::vector<TowerInfoDefinition>& GetDefinitions() const;
				virtual const TowerInfoDefinition* GetDefinition(const std::string& towerType) const;
				virtual const TowerInfoDefinition* GetDefinition(uint64_t towerId) const;
				virtual void UseJsonData(nlohmann::json content);
				virtual void PreloadRuntime();
				virtual void FinalizeTowerRegistration(const Util::FlagManager& towerFlags);
				bool IsRuntimePreloaded() const { return runtimePreloaded; }
				virtual bool BindDefinitionId(const std::string& towerType, uint64_t towerId);
				
				virtual bool ShouldDisplayInInfoPanel(const std::string& towerType, bool isCustomTower) const;
				virtual bool ShouldDisplayInInfoPanel(uint64_t towerId, const std::string& towerType, bool isCustomTower) const;
				virtual bool ShouldHideUpgradeUnlocks(const std::string& towerType) const;
				virtual bool ShouldHideUpgradeUnlocks(uint64_t towerId, const std::string& towerType) const;
				virtual bool CanUnlockTower(const std::string& towerType) const;
				virtual bool CanUnlockTower(uint64_t towerId, const std::string& towerType) const;

				static bool IsVanillaTowerInfoTower(uint64_t towerId, const std::string& towerType);
				static bool IsCustomTowerInfoTower(uint64_t towerId);
			};
		}
	}
}
