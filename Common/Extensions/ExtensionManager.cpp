#include "ExtensionManager.h"

#include "Bloon/BloonFlagsExt.h"
#include "Generic/FlagsExtension.h"
#include "Generic/MergeIgnoreExtension.h"
#include "StatusEffect/StatusFlagsExt.h"
#include "Textures/SheetsExtension.h"
#include "Tower/TowerFlagsExt.h"
#include "StatusEffect/StatusFlagsExt.h"
#include "Weapon/WeaponFlagsExt.h"

#include <Logging/Logger.h>

using namespace Common::Extensions;
using namespace Common::Logging::Logger;

namespace
{
	bool MatchesExtensionTarget(const std::string& pattern, const std::string& target)
	{
		size_t patternPos = 0;
		size_t targetPos = 0;
		size_t starPos = std::string::npos;
		size_t retryTargetPos = 0;

		while (targetPos < target.size())
		{
			if (patternPos < pattern.size() &&
				(pattern[patternPos] == '?' || pattern[patternPos] == target[targetPos]))
			{
				++patternPos;
				++targetPos;
			}
			else if (patternPos < pattern.size() && pattern[patternPos] == '*')
			{
				starPos = patternPos++;
				retryTargetPos = targetPos;
			}
			else if (starPos != std::string::npos)
			{
				patternPos = starPos + 1;
				targetPos = ++retryTargetPos;
			}
			else
			{
				return false;
			}
		}

		while (patternPos < pattern.size() && pattern[patternPos] == '*')
			++patternPos;

		return patternPos == pattern.size();
	}
}

void ExtensionManager::AddAll()
{
	ExtensionManager::AddExtension<Bloon::BloonFlagExt>();
	ExtensionManager::AddExtension<Generic::MergeIgnoreExtension>();
	ExtensionManager::AddExtension<StatusEffect::StatusFlagsExt>();
	ExtensionManager::AddExtension<Textures::SheetsExtension>();
	ExtensionManager::AddExtension<Tower::TowerFlagExt>();
	ExtensionManager::AddExtension<Weapon::WeaponFlagsExt>();
}

Extension* ExtensionManager::GetByName(const std::string& name) {
	for (auto** storage : storages) {
		auto* extension = *storage;
		if (extension->GetName() == name) {
			return extension;
		}
	}
	return nullptr;
}

std::vector<Extension*> ExtensionManager::GetByTarget(const std::string& target)
{
	std::vector<Extension*> results;
	for (auto** storage : storages) {
		auto* extension = *storage;
		if (MatchesExtensionTarget(extension->GetTarget(), target)) {
			results.push_back(extension);
		}
	}
	return results;
}

std::vector<Extension*> ExtensionManager::GetCustomDocuments()
{
	std::vector<Extension*> results;
	for (auto** storage : storages) {
		auto* extension = *storage;
		if (extension->IsCustomDocument()) {
			results.push_back(extension);
		}
	}
	return results;
}
