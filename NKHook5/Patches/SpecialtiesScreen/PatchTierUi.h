#pragma once

#include "../IPatch.h"

namespace NKHook5::Patches::SpecialtiesScreen
{
	class PatchTierUi : public IPatch
	{
	public:
		PatchTierUi() : IPatch("SpecialtiesScreen::PatchTierUi") {}
		bool Apply() override;
	};
}
