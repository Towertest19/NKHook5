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

		if (labExt)
			labExt->RecordLabShopQuery(labType, vanillaMax);
		if (specExt)
			specExt->RecordSpecialtyShopQuery(labType, vanillaMax);

		const bool specialtyQuery = IsSpecialtyVanillaCap(vanillaMax);
		const bool monkeyLabQuery = IsMonkeyLabVanillaCap(vanillaMax);

		// Scope overrides by the vanilla cap the game uses for each screen:
		// monkey labs (LabShop / LabDefinitions) vs specialties (SpecialtyShop).
		if (specialtyQuery && specExt)
		{
			const int dynMax = specExt->GetMaxLevel(labType);
			if (dynMax > 0)
			{
				Print(LogLevel::INFO,
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
				Print(LogLevel::INFO,
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
		if (monkeyLabQuery && labExt)
		{
			const int dynMax = labExt->GetFallbackMaxLevel(vanillaMax, labType);
			if (dynMax > fallbackMax)
				fallbackMax = dynMax;
		}

		if (fallbackMax != vanillaMax)
		{
			Print(LogLevel::INFO,
				"GetMaxLevel(%d): scoped fallback extended vanilla %d -> %d",
				labType, vanillaMax, fallbackMax);
		}
		return fallbackMax;
	}

	auto GetMaxLevel::Apply() -> bool
	{
		Print(LogLevel::INFO, "GetMaxLevel patch: locating CLabFactory::GetMaxLevel...");

		const void* address = Signatures::GetAddressOf(Sigs::CLabFactory_GetMaxLevel);
		if (address)
		{
			Print(LogLevel::INFO, "GetMaxLevel patch: found at %p, applying hook...", address);

			{
				MEMORY_BASIC_INFORMATION mbi{};
				if (VirtualQuery(address, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT)
				{
					const DWORD prot = (mbi.Protect & 0xFF);
					const bool readable =
						(prot == PAGE_READONLY)            ||
						(prot == PAGE_READWRITE)           ||
						(prot == PAGE_WRITECOPY)           ||
						(prot == PAGE_EXECUTE_READ)        ||
						(prot == PAGE_EXECUTE_READWRITE)   ||
						(prot == PAGE_EXECUTE_WRITECOPY);
					if (readable)
					{
						unsigned char b[8]{};
						memcpy(b, address, sizeof(b));
						Print(LogLevel::INFO,
							"GetMaxLevel patch: target bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
							b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
					}
					else
					{
						Print(LogLevel::WARNING,
							"GetMaxLevel patch: target not readable (protect=0x%X)",
							static_cast<unsigned>(mbi.Protect));
					}
				}
				else
				{
					Print(LogLevel::WARNING,
						"GetMaxLevel patch: VirtualQuery failed for %p", address);
				}
			}

			auto* detour = new PLH::x86Detour(
				reinterpret_cast<uintptr_t>(address),
				std::bit_cast<size_t>(&cb_hook_getMaxLevel),
				&o_func
			);

			if (detour->hook())
			{
				SetVanillaGetMaxLevel(reinterpret_cast<VanillaGetMaxLevelFn>(o_func));
				Print(LogLevel::INFO,
					"GetMaxLevel patch: hooked successfully. "
					"Dynamic lab/specialty levels active with scoped fallbacks.");
				return true;
			}
			else
			{
				Print(LogLevel::ERR,
					"GetMaxLevel patch: failed to hook at %p", address);
				return false;
			}
		}
		else
		{
			Print(LogLevel::WARNING, "GetMaxLevel patch: signature not found");
			Print(LogLevel::INFO,
				"GetMaxLevel patch: SpecialtyDefinitionsExt is still loaded "
				"and available for name-based queries.");
			return false;
		}
	}
}
