#pragma once

#include "../IPatch.h"
#include "../../Utils.h"

namespace NKHook5::Patches::TowerInfoScreen
{
	using namespace NKHook5;

	class SetTower : public IPatch
	{
		void cb_hook(void* thisptr, int pad, uint64_t towerId);
	public:
		SetTower() : IPatch("TowerInfoScreen::SetTower") {};
		bool Apply() override;
	};
}
