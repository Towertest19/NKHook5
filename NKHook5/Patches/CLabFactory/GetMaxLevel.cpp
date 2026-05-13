#include "GetMaxLevel.h"

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

	GetMaxLevel::GetMaxLevel() : IPatch("CLabFactory::GetMaxLevel") {}

	// CLabFactory::GetMaxLevel hook.
	//
	// The game function is __thiscall and takes a single INTEGER lab-type index
	// (NOT a const char* pointer).  It subtracts 0x25 from the argument and
	// dispatches through a jump table to return the per-lab constant max level
	// (values 1–21, with 13 as the vanilla out-of-range default).
	//
	// With NKHook5 active the max level is DYNAMIC:
	//   1. LabDefinitionsExt is queried first so MonkeyLabScreen can extend
	//      regular lab caps directly from the JSON Upgrades array.
	//   2. SpecialtyDefinitionsExt is queried next. If a SpecialtyDefinitions
	//      JSON in Assets/JSON/SpecialtyDefinitions/ declares a matching LabType,
	//      the level derived from its Roman-numeral Effects keys is returned
	//      (I=1 … IX=9, "X" drawback excluded). This makes it possible to
	//      extend any specialty building to up to 9 upgrade tiers.
	//   3. If no override is registered the original function is called so all
	//      vanilla lab and specialty buildings continue to work unchanged.
	//
	// __fastcall -> __thiscall ABI bridge (x86):
	//   ECX    -> thisptr   (CLabFactory instance)
	//   EDX    -> pad       (ignored, required by __fastcall convention)
	//   [SP+4] -> labType   (integer lab-type index passed by the game)
	int __fastcall cb_hook_getMaxLevel(void* thisptr, int /*pad*/, int labType)
	{
		// ── Step 1: check LabDefinitionsExt ──────────────────────────────────
		// MonkeyLabScreen asks for the same integer lab-type max level as the
		// factory.  Let LabDefinitions JSON extend that cap from its Upgrades
		// array before falling through to specialty metadata or vanilla logic.
		auto* labExt = ExtensionManager::Get<LabDefinitionsExt>();
		if (labExt)
		{
			const int dynMax = labExt->GetMaxLevel(labType);
			if (dynMax > 0)
			{
				Print(LogLevel::INFO,
					"GetMaxLevel(%d): dynamic override -> %d (LabDefinitionsExt)",
					labType, dynMax);
				return dynMax;
			}
		}

		// ── Step 2: check SpecialtyDefinitionsExt ────────────────────────────
		// A positive return value means a JSON definition was found whose
		// Effects map has at least one upgrade tier.  Return it directly.
		auto* specExt = ExtensionManager::Get<SpecialtyDefinitionsExt>();
		if (specExt)
		{
			const int dynMax = specExt->GetMaxLevel(labType);
			if (dynMax > 0)
			{
				Print(LogLevel::INFO,
					"GetMaxLevel(%d): dynamic override -> %d (SpecialtyDefinitionsExt)",
					labType, dynMax);
				return dynMax;
			}
		}

		// ── Step 3: ask vanilla, then let untyped JSON definitions extend it ──
		// The vanilla function is a safe integer switch with a bounds check,
		// so it cannot crash on an unrecognised labType. Some mods do not know the
		// integer labType assigned by the game; in that case we still honour loaded
		// JSON definitions by extending the vanilla cap to the highest JSON tier.
		auto ofn = reinterpret_cast<int(__thiscall*)(void*, int)>(o_func);
		const int vanillaMax = ofn(thisptr, labType);

		int fallbackMax = vanillaMax;
		if (labExt)
		{
			const int dynMax = labExt->GetFallbackMaxLevel(vanillaMax);
			if (dynMax > fallbackMax)
				fallbackMax = dynMax;
		}
		if (specExt)
		{
			const int dynMax = specExt->GetFallbackMaxLevel(vanillaMax);
			if (dynMax > fallbackMax)
				fallbackMax = dynMax;
		}

		if (fallbackMax != vanillaMax)
		{
			Print(LogLevel::INFO,
				"GetMaxLevel(%d): untyped JSON fallback extended vanilla %d -> %d",
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

			// Log the first 8 bytes at the target for sanity verification.
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
				Print(LogLevel::INFO,
					"GetMaxLevel patch: hooked successfully. "
					"Dynamic lab/specialty levels active (labs from Upgrades, specialties up to IX = 9 tiers).");
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
