#pragma once

#include <Extensions/JsonExtension.h>
#include <nlohmann/json.hpp>
#include <array>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NKHook5::Extensions::SpecialtyDefinitions
{
	// Vanilla specialty buildings all cap at tier IV.
	static constexpr int kVanillaMaxLevel = 4;

	// One entry per SpecialtyDefinitions JSON file.
	struct SpecialtyDefinition
	{
		// LOC key, e.g. "LOC_SPEC_Colt"
		std::string name;

		// The integer the game passes to CLabFactory::GetMaxLevel for this
		// specialty.  -1 means not yet mapped; such definitions can only be
		// looked up by name, not by labType.
		int labType;

		// Highest present Roman-numeral tier (I=1 … IX=9).  "X" is the
		// drawback tier and is always excluded from this count.
		int maxLevel;

		// Sorted list of present tier keys, e.g. {"I","II","III","IV","V"}.
		// Populated at load time; useful for logging and UI introspection.
		std::vector<std::string> tiers;

		SpecialtyDefinition() : labType(-1), maxLevel(kVanillaMaxLevel) {}
	};

	class SpecialtyDefinitionsExt : public Common::Extensions::JsonExtension
	{
	private:
		std::vector<SpecialtyDefinition>        definitions;
		std::unordered_map<std::string, size_t> nameToIndex;
		std::unordered_map<int, size_t>         labTypeToIndex;

		// Count the highest present Roman-numeral tier in an Effects object.
		// Returns 0 if the object is absent or empty.
		static int CountTiers(const nlohmann::json& effects);

	public:
		SpecialtyDefinitionsExt();

		void UseJsonData(nlohmann::json content) override;
		void FinalizeTowerRegistration();

		// Returns the dynamic max level for the given game labType integer,
		// or -1 if no definition is registered for that labType.
		// A result of -1 means the caller should fall through to the original.
		int GetMaxLevel(int labType) const;

		// Returns the highest JSON-derived tier that should extend an otherwise
		// unmapped/custom specialty lab type, or -1 if no loaded definition extends it.
		int GetFallbackMaxLevel(int vanillaMaxLevel) const;

		// Returns the dynamic max level by specialty name (LOC key or bare
		// name), or -1 if not registered.
		int GetMaxLevel(const std::string& name) const;

		// Full definition access.
		const SpecialtyDefinition* GetDefinition(int labType) const;
		const SpecialtyDefinition* GetDefinition(const std::string& name) const;
		const std::vector<SpecialtyDefinition>& GetDefinitions() const;
	};
}
