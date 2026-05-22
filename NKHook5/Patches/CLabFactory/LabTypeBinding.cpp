#include "LabTypeBinding.h"

#include "../../Extensions/LabDefinitions/LabDefinitionsExt.h"
#include "../../Extensions/SpecialtyDefinitions/SpecialtyDefinitionsExt.h"

using namespace NKHook5::Extensions::LabDefinitions;
using namespace NKHook5::Extensions::SpecialtyDefinitions;

namespace NKHook5::Patches::CLabFactory
{
	namespace
	{
		bool g_shopOrdersReady = false;
	}

	void SetVanillaGetMaxLevel(VanillaGetMaxLevelFn fn)
	{
		(void)fn;
	}

	void TryBindLabTypes(void* /*labFactory*/,
		LabDefinitionsExt* labExt,
		SpecialtyDefinitionsExt* specExt)
	{
		if (g_shopOrdersReady)
			return;
		g_shopOrdersReady = true;

		// Lab/specialty runtime preload is handled by RuntimeHooks after tower registration.
	}
}
