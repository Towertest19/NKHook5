#include "GetMaxLevel.h"
#include "LabTypeBinding.h"

#include "../../Extensions/ExtensionManager.h"
#include "../../Extensions/LabDefinitions/LabDefinitionsExt.h"
#include "../../Extensions/SpecialtyDefinitions/SpecialtyDefinitionsExt.h"
#include "../../Signatures/Signature.h"
#include "../../Classes/Macro.h"
#include <Logging/Logger.h>

#include <polyhook2/Detour/x86Detour.hpp>

#include <windows.h>

namespace NKHook5::Patches::CLabFactory
{
	using namespace Common;
	using namespace Common::Extensions;
	using namespace Common::Logging;
	using namespace Common::Logging::Logger;
	using namespace NKHook5;
	using namespace NKHook5::Extensions;
	using namespace NKHook5::Extensions::LabDefinitions;
	using namespace NKHook5::Extensions::SpecialtyDefinitions;
	using namespace Signatures;

	static uint64_t o_func;

	namespace
	{
		constexpr int kSpecialtyVanillaCap = 4;

		bool IsMonkeyLabVanillaCap(int vanillaMax)
		{
			return (vanillaMax >= 10 && vanillaMax <= 21) || vanillaMax == 13;
		}

		bool IsSpecialtyVanillaCap(int vanillaMax)
		{
			return vanillaMax == kSpecialtyVanillaCap;
		}
	}

	GetMaxLevel::GetMaxLevel() : IPatch("CLabFactory::GetMaxLevel") {}

	// CLabFactory::GetMaxLevel hook.
	//
	// The game passes an integer lab-type index (labType), not a name pointer.
	// Flow:
	//   1. Call vanilla so we know the stock cap for this labType.
	//   2. Lazy-bind JSON definitions that omitted LabType (common in mods).
	//   3. Return a typed LabDefinitions / SpecialtyDefinitions override when bound.
	//   4. Otherwise apply a scoped fallback (labs vs specialties by vanilla cap).
	int __fastcall cb_hook_getMaxLevel(void* thisptr, int /*pad*/, int labType)
	{
		auto ofn = reinterpret_cast<VanillaGetMaxLevelFn>(o_func);
		const int vanillaMax = ofn(thisptr, labType);

		auto* labExt = ExtensionManager::Get<LabDefinitionsExt>();
		auto* specExt = ExtensionManager::Get<SpecialtyDefinitionsExt>();

		TryBindLabTypes(thisptr, labExt, specExt);

		const bool specialtyQuery = IsSpecialtyVanillaCap(vanillaMax);
		const bool monkeyLabQuery = IsMonkeyLabVanillaCap(vanillaMax);

		// Scope typed overrides by the vanilla cap the game uses for each definition type.
		if (specialtyQuery && specExt)
		{
			const int dynMax = specExt->GetMaxLevel(labType);
			if (dynMax > 0)
			{
				Print(LogLevel::DEBUG,
					"GetMaxLevel(%d): override -> %d (SpecialtyDefinitionsExt, vanilla was %d)",
					labType, dynMax, vanillaMax);
				return dynMax;
			}
		}

		if (monkeyLabQuery && labExt)
		{
			const int dynMax = labExt->GetMaxLevel(labType);
			if (dynMax > 0)
			{
				Print(LogLevel::DEBUG,
					"GetMaxLevel(%d): override -> %d (LabDefinitionsExt, vanilla was %d)",
					labType, dynMax, vanillaMax);
				return dynMax;
			}
		}

		int fallbackMax = vanillaMax;
		if (specialtyQuery && specExt)
		{
			const int dynMax = specExt->GetFallbackMaxLevel(vanillaMax, labType);
			if (dynMax > fallbackMax)
				fallbackMax = dynMax;
		}
		else if (monkeyLabQuery && labExt)
		{
			const int dynMax = labExt->GetFallbackMaxLevel(vanillaMax, labType);
			if (dynMax > fallbackMax)
				fallbackMax = dynMax;
			else if (dynMax <= 0)
			{
				const int highestLabMax = labExt->GetHighestDefinedMaxLevel();
				if (highestLabMax > fallbackMax)
					fallbackMax = highestLabMax;
			}
		}

		if (fallbackMax != vanillaMax)
		{
			Print(LogLevel::DEBUG,
				"GetMaxLevel(%d): scoped fallback extended vanilla %d -> %d",
				labType, vanillaMax, fallbackMax);
		}
		return fallbackMax;
	}

	auto GetMaxLevel::Apply() -> bool
	{
		Print(LogLevel::DEBUG, "GetMaxLevel patch: locating CLabFactory::GetMaxLevel...");

		const void* address = Signatures::GetAddressOf(Sigs::CLabFactory_GetMaxLevel);
		if (address)
		{
			Print(LogLevel::DEBUG, "GetMaxLevel patch: applying hook...");


			auto* detour = new PLH::x86Detour(
				reinterpret_cast<uintptr_t>(address),
				std::bit_cast<size_t>(&cb_hook_getMaxLevel),
				&o_func
			);

			if (detour->hook())
			{
				SetVanillaGetMaxLevel(reinterpret_cast<VanillaGetMaxLevelFn>(o_func));
				Print(LogLevel::DEBUG,
					"GetMaxLevel patch: hooked successfully. "
					"Dynamic lab/specialty levels active with scoped fallbacks.");
				return true;
			}
			else
			{
				Print(LogLevel::ERR, "GetMaxLevel patch: failed to hook");
				return false;
			}
		}
		else
		{
			Print(LogLevel::WARNING, "GetMaxLevel patch: signature not found");
			Print(LogLevel::DEBUG,
				"GetMaxLevel patch: SpecialtyDefinitionsExt is still loaded "
				"and available for name-based queries.");
			return false;
		}
	}
}
