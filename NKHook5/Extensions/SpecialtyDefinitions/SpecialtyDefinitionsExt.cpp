#include "SpecialtyDefinitionsExt.h"

#include <Logging/Logger.h>

using namespace Common;
using namespace Common::Extensions;
using namespace Common::Logging::Logger;
using namespace NKHook5;
using namespace NKHook5::Extensions;
using namespace NKHook5::Extensions::SpecialtyDefinitions;

// ---------------------------------------------------------------------------
// Ordered Roman-numeral upgrade tier keys, I through IX.
// "X" is the drawback / penalty tier and is deliberately absent from this table.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------

SpecialtyDefinitionsExt::SpecialtyDefinitionsExt()
	: JsonExtension("SpecialtyDefinitions", "Assets/JSON/SpecialtyDefinitions/*.json")
{
}

/*static*/ int SpecialtyDefinitionsExt::CountTiers(const nlohmann::json& effects)
{
	if (!effects.is_object())
		return 0;

	int highest = 0;
	for (const auto& [key, tierIdx] : kTierOrder)
	{
		if (effects.contains(key) && tierIdx > highest)
			highest = tierIdx;
	}
	return highest;
}

void SpecialtyDefinitionsExt::UseJsonData(nlohmann::json content)
{
	if (content.empty())
	{
		Print(LogLevel::ERR, "SpecialtyDefinitions: received empty JSON");
		return;
	}

	if (!content.contains("Name"))
	{
		Print(LogLevel::ERR, "SpecialtyDefinitions: JSON missing required 'Name' field");
		return;
	}

	try
	{
		SpecialtyDefinition def;
		def.name = content["Name"].get<std::string>();

		// LabType is an NKHook5 extension field: the integer the game passes
		// to CLabFactory::GetMaxLevel for this specialty building.
		// Omitting it (or setting -1) means the definition is name-only.
		def.labType = content.value("LabType", -1);

		if (content.contains("Effects") && content["Effects"].is_object())
		{
			const auto& effects = content["Effects"];
			def.maxLevel = CountTiers(effects);

			// Record which upgrade tier keys are present for logging.
			for (const auto& [key, tierIdx] : kTierOrder)
			{
				if (effects.contains(key))
					def.tiers.emplace_back(key);
			}

			if (def.maxLevel == 0)
			{
				// Effects map exists but has no recognised upgrade keys
				// (maybe only "X" is present).  Fall back to vanilla default.
				def.maxLevel = kVanillaMaxLevel;
				Print(LogLevel::WARNING,
					"SpecialtyDef '%s': Effects map has no upgrade tiers, using vanilla default %d",
					def.name.c_str(), def.maxLevel);
			}
		}
		else
		{
			// No Effects map — respect an explicit MaxLevel override if present,
			// otherwise use the vanilla default of 4 (tier IV).
			def.maxLevel = content.value("MaxLevel", kVanillaMaxLevel);
		}

		// Build tier summary string for the log line.
		std::string tierSummary;
		for (size_t i = 0; i < def.tiers.size(); ++i)
		{
			if (i) tierSummary += ',';
			tierSummary += def.tiers[i];
		}
		if (tierSummary.empty()) tierSummary = "(none)";

		Print(LogLevel::INFO,
			"SpecialtyDef loaded: name='%s'  labType=%d  maxLevel=%d  tiers=[%s]",
			def.name.c_str(), def.labType, def.maxLevel, tierSummary.c_str());

		const size_t idx = definitions.size();

		// Register by full name.
		nameToIndex[def.name] = idx;

		// Also register by bare name (strip "LOC_SPEC_" prefix) for convenience.
		{
			static constexpr const char* kPrefix    = "LOC_SPEC_";
			static constexpr size_t      kPrefixLen = 9; // strlen("LOC_SPEC_")
			if (def.name.rfind(kPrefix, 0) == 0 && def.name.size() > kPrefixLen)
				nameToIndex[def.name.substr(kPrefixLen)] = idx;
		}

		// Register by labType if one was specified.
		if (def.labType >= 0)
		{
			if (labTypeToIndex.count(def.labType))
			{
				Print(LogLevel::WARNING,
					"SpecialtyDef: labType=%d already registered, overwriting with '%s'",
					def.labType, def.name.c_str());
			}
			labTypeToIndex[def.labType] = idx;
		}

		definitions.emplace_back(std::move(def));
	}
	catch (const std::exception& e)
	{
		Print(LogLevel::ERR, "SpecialtyDefinitions: failed to parse JSON: %s", e.what());
	}
}

int SpecialtyDefinitionsExt::GetMaxLevel(int labType) const
{
	const auto it = labTypeToIndex.find(labType);
	if (it != labTypeToIndex.end())
		return definitions[it->second].maxLevel;
	return -1; // not registered — caller should fall through to original
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
