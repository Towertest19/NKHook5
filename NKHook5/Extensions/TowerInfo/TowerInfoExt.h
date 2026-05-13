#pragma once

#include <Extensions/JsonExtension.h>

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
				bool hideUpgradeUnlocks = false; // Hide upgrade unlock notifications
				std::string customDescription;  // Optional custom description
			};

			class TowerInfoExt : public JsonExtension
			{
				std::vector<TowerInfoDefinition> definitions;
				std::unordered_map<std::string, size_t> nameToIndex;
				std::unordered_map<uint64_t, size_t> idToIndex;
				bool loadedAny = false;
			public:
				TowerInfoExt();
				virtual const std::vector<TowerInfoDefinition>& GetDefinitions() const;
				virtual const TowerInfoDefinition* GetDefinition(const std::string& towerType) const;
				virtual const TowerInfoDefinition* GetDefinition(uint64_t towerId) const;
				virtual void UseJsonData(nlohmann::json content);
				virtual void FinalizeTowerRegistration(const Util::FlagManager& towerFlags);
				
				// Check if tower should be displayed in info panel
				// For vanilla towers: always true (backward compatible)
				// For custom towers: explicit CanBeViewed=false hides; otherwise show.
				virtual bool ShouldDisplayInInfoPanel(const std::string& towerType, bool isCustomTower) const;
				virtual bool ShouldDisplayInInfoPanel(uint64_t towerId, const std::string& towerType, bool isCustomTower) const;
				
				// Check if tower should hide upgrade unlock notifications
				virtual bool ShouldHideUpgradeUnlocks(const std::string& towerType) const;
			};
		}
	}
}
