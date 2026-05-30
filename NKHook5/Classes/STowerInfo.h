#pragma once

#include <cstdint>

namespace NKHook5::Classes
{
	struct STowerUpgradeInfo
	{
		char pad_0000[0x18];
	};

	struct STowerUpgradePath
	{
		STowerUpgradeInfo* begin = nullptr;
		STowerUpgradeInfo* end = nullptr;
		STowerUpgradeInfo* capacity = nullptr;
	};

	struct STowerInfo
	{
		char pad_0000[0x98];
		STowerUpgradePath upgradePaths[2];

		int32_t GetUpgradeCount(int32_t path) const
		{
			if (path < 0 || path >= 2)
				return 0;

			const STowerUpgradePath& upgradePath = upgradePaths[path];
			const uintptr_t begin = reinterpret_cast<uintptr_t>(upgradePath.begin);
			const uintptr_t end = reinterpret_cast<uintptr_t>(upgradePath.end);
			if (begin == 0 || end < begin)
				return 0;

			const uintptr_t bytes = end - begin;
			return static_cast<int32_t>(bytes / sizeof(STowerUpgradeInfo));
		}
	};

	static_assert(sizeof(STowerUpgradeInfo) == 0x18);
	static_assert(sizeof(STowerUpgradePath) == 0x0C);
}