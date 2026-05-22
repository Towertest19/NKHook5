#pragma once

#include <string>

namespace NKHook5::Patches::RuntimeHooks
{
	void PrimeLabSpecialtyAfterMods();
	void NotifyTowerTypesReady();
	void OnMergedAssetLoaded(const std::string& assetPath);
}
