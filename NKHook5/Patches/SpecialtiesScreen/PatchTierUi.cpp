#include "PatchTierUi.h"

#include "../../Signatures/Signature.h"

#include <Logging/Logger.h>

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <limits>

namespace NKHook5::Patches::SpecialtiesScreen
{
	using namespace Common::Logging::Logger;

	namespace
	{
		bool WritePatch(void* target, const void* bytes, size_t len)
		{
			DWORD oldProtect = 0;
			if (!VirtualProtect(target, len, PAGE_EXECUTE_READWRITE, &oldProtect))
				return false;

			std::memcpy(target, bytes, len);
			FlushInstructionCache(GetCurrentProcess(), target, len);

			DWORD ignored = 0;
			VirtualProtect(target, len, oldProtect, &ignored);
			return true;
		}
	}

	bool PatchTierUi::Apply()
	{
		const void* switchSite = Signatures::GetAddressOf(Signatures::Sigs::SpecialtiesScreen_TierSwitch);
		if (!switchSite)
		{
			Print(LogLevel::WARNING, "SpecialtiesScreen tier patch: tier switch signature not found");
			return false;
		}

		const auto* siteBytes = reinterpret_cast<const uint8_t*>(switchSite);
		const uint32_t oldJumpTable = *reinterpret_cast<const uint32_t*>(siteBytes + 18);
		const uint32_t dynamicTextHandler = *reinterpret_cast<const uint32_t*>(siteBytes + 0xD6);
		const auto* oldTable = reinterpret_cast<const uint32_t*>(oldJumpTable);

		static uint32_t extendedJumpTable[9]{};
		for (size_t i = 0; i < 4; ++i)
			extendedJumpTable[i] = oldTable[i];
		for (size_t i = 4; i < 9; ++i)
			extendedJumpTable[i] = dynamicTextHandler;

		const uint8_t maxCase = 8;
		if (!WritePatch(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(switchSite) + 8), &maxCase, sizeof(maxCase)))
		{
			Print(LogLevel::ERR, "SpecialtiesScreen tier patch: failed to raise tier cap");
			return false;
		}

		const uintptr_t tablePointer = reinterpret_cast<uintptr_t>(extendedJumpTable);
		if (tablePointer > (std::numeric_limits<uint32_t>::max)())
		{
			Print(LogLevel::ERR, "SpecialtiesScreen tier patch: extended tier table is outside 32-bit address space");
			return false;
		}

		const uint32_t tableAddress = static_cast<uint32_t>(tablePointer);
		if (!WritePatch(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(switchSite) + 18), &tableAddress, sizeof(tableAddress)))
		{
			Print(LogLevel::ERR, "SpecialtiesScreen tier patch: failed to redirect tier table");
			return false;
		}


		Print(LogLevel::DEBUG, "SpecialtiesScreen tier patch: runtime cap extended to IX when JSON max tiers require it");
		return true;
	}
}
