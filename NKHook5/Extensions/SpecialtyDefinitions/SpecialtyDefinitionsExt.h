#pragma once

#include <Extensions/JsonExtension.h>
#include <nlohmann/json.hpp>
#include <array>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstdint>

#include <cstddef>

namespace NKHook5::Extensions::SpecialtyDefinitions
{
	static constexpr int kVanillaMaxLevel = 4;

	struct SpecialtyDefinition
	{
		std::string name;
		std::string fileName;
		std::string building;
		int labType;
		int maxLevel;
		std::vector<std::string> tiers;

		SpecialtyDefinition() : labType(-1), maxLevel(kVanillaMaxLevel) {}
	};

	class SpecialtyDefinitionsExt : public Common::Extensions::JsonExtension
	{
	private:
		std::vector<SpecialtyDefinition>        definitions;
		std::unordered_map<std::string, size_t> nameToIndex;
		std::unordered_map<int, size_t>         labTypeToIndex;

		std::vector<std::string> specialtyShopOrder;
		std::unordered_map<std::string, std::string> specialtyShopTypeAliases;
		std::unordered_map<std::string, int> buildingToLabType;
		size_t specialtyRecordIndex = 0;
		bool modSpecialtyTypesApplied = false;
		bool runtimePreloaded = false;

		static int CountTiers(const nlohmann::json& effects, bool clampToRuntimeMax = true);
		static bool ShouldSkipJson(const nlohmann::json& content);
		size_t UpsertDefinition(SpecialtyDefinition def);

	public:
		SpecialtyDefinitionsExt();

		void UseJsonData(nlohmann::json content) override;
		void PreloadRuntime();
		void LoadSpecialtyShopOrder();
		bool IsRuntimePreloaded() const { return runtimePreloaded; }

		void RecordSpecialtyShopQuery(int labType, int vanillaMaxLevel);
		void ApplyModSpecialtyBindings();

		int GetMaxLevel(int labType) const;
		int GetFallbackMaxLevel(int vanillaMaxLevel, int labType) const;
		int GetMaxLevel(const std::string& name) const;

		const SpecialtyDefinition* GetDefinition(int labType) const;
		const SpecialtyDefinition* GetDefinition(const std::string& name) const;
		const std::vector<SpecialtyDefinition>& GetDefinitions() const;

		bool AugmentSpecialtyShopJson(nlohmann::json& root) const;
	};
}
