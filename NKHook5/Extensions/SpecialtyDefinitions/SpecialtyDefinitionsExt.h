#pragma once

#include <Extensions/JsonExtension.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

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
		size_t priceCount;
		std::vector<std::string> tiers;

		SpecialtyDefinition() : labType(-1), maxLevel(kVanillaMaxLevel), priceCount(0) {}
	};

	class SpecialtyDefinitionsExt : public Common::Extensions::JsonExtension
	{
	private:
		std::vector<SpecialtyDefinition>        definitions;
		std::unordered_map<std::string, size_t> nameToIndex;
		std::unordered_map<int, size_t>         labTypeToIndex;

		bool runtimePreloaded = false;

		static int CountTiers(const nlohmann::json& effects, bool clampToRuntimeMax = true);
		static bool ShouldSkipJson(const nlohmann::json& content);
		size_t UpsertDefinition(SpecialtyDefinition def);

	public:
		SpecialtyDefinitionsExt();

		void UseJsonData(nlohmann::json content) override;
		void PreloadRuntime();
		bool IsRuntimePreloaded() const { return runtimePreloaded; }

		bool HasBoundDefinition(int labType) const;
		int GetMaxLevel(int labType) const;
		int GetFallbackMaxLevel(int vanillaMaxLevel, int labType) const;
		int GetHighestDefinedMaxLevel() const;
		int GetMaxLevel(const std::string& name) const;
		bool HasTier(int labType, int tier) const;
		bool HasTier(const std::string& name, int tier) const;

		const SpecialtyDefinition* GetDefinition(int labType) const;
		const SpecialtyDefinition* GetDefinition(const std::string& name) const;
		const std::vector<SpecialtyDefinition>& GetDefinitions() const;
	};
}
