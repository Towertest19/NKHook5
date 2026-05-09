#pragma once

#include "../../Patches/IPatch.h"
#include "../../Extensions/LabDefinitions/LabDefinitionsExt.h"

namespace NKHook5::Patches::CLabFactory
{
	class GetMaxLevel : public IPatch
	{
	public:
		GetMaxLevel();
		auto Apply() -> bool override;
	};
}
