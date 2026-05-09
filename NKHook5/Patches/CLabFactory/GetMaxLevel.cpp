#include "GetMaxLevel.h"

#include "../../Extensions/ExtensionManager.h"
#include "../../Extensions/LabDefinitions/LabDefinitionsExt.h"
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
	using namespace Signatures;

	static uint64_t o_func;

	GetMaxLevel::GetMaxLevel() : IPatch("CLabFactory::GetMaxLevel") {}
	
	// Hook function that intercepts max level calculations
	int __fastcall cb_hook_getMaxLevel(void* thisptr, int pad, const char* labName)
	{
		Print(LogLevel::INFO, "CLabFactory::GetMaxLevel hook entered (this=%p, labName=%p)", thisptr, labName);
		if (labName)
		{
			Print(LogLevel::INFO, "CLabFactory::GetMaxLevel labName='%s'", labName);
		}
		else
		{
			Print(LogLevel::WARNING, "CLabFactory::GetMaxLevel labName is null");
		}

		// Get our LabDefinitions extension
		auto* labDefsExt = ExtensionManager::Get<LabDefinitionsExt>();
		if (labDefsExt)
		{
			// Calculate max level dynamically from JSON
			int dynamicMaxLevel = labDefsExt->GetMaxLevel(std::string(labName));
			Print(LogLevel::INFO, "Lab '%s': using dynamic max level = %d", labName, dynamicMaxLevel);
			return dynamicMaxLevel;
		}
		
		// Fallback to original function if extension not available
		Print(LogLevel::WARNING, "LabDefinitions extension not available, using original max level for '%s'", labName);
		return ((int(__thiscall*)(void*, const char*))o_func)(thisptr, labName);
	}

	auto GetMaxLevel::Apply() -> bool
	{
		Print(LogLevel::INFO, "Lab max level patch: Looking for lab max level function...");
		
		const void* address = Signatures::GetAddressOf(Sigs::CLabFactory_GetMaxLevel);
		if (address)
		{
			Print(LogLevel::INFO, "Lab max level patch: Found function at %p, applying hook...", address);
			{
				MEMORY_BASIC_INFORMATION mbi{};
				if (VirtualQuery(address, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT)
				{
					const DWORD prot = (mbi.Protect & 0xFF);
					const bool readable = (prot == PAGE_READONLY) || (prot == PAGE_READWRITE) || (prot == PAGE_WRITECOPY) ||
						(prot == PAGE_EXECUTE_READ) || (prot == PAGE_EXECUTE_READWRITE) || (prot == PAGE_EXECUTE_WRITECOPY);
					if (readable)
					{
						unsigned char b[8]{};
						memcpy(b, address, sizeof(b));
						Print(LogLevel::INFO, "Lab max level patch: target bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
							b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
					}
					else
					{
						Print(LogLevel::WARNING, "Lab max level patch: target memory is not readable (protect=0x%X)", (unsigned)mbi.Protect);
					}
				}
				else
				{
					Print(LogLevel::WARNING, "Lab max level patch: VirtualQuery failed for %p", address);
				}
			}
			
			auto* detour = new PLH::x86Detour(
				(const uintptr_t)address, 
				std::bit_cast<size_t>(&cb_hook_getMaxLevel), 
				&o_func
			);
			
			if (detour->hook())
			{
				Print(LogLevel::INFO, "Lab max level patch: Successfully hooked! Dynamic max levels active.");
				return true;
			}
			else
			{
				Print(LogLevel::ERR, "Lab max level patch: Failed to hook function at %p", address);
				return false;
			}
		}
		else
		{
			Print(LogLevel::WARNING, "Lab max level patch: Could not find function signature");
			Print(LogLevel::INFO, "Lab max level patch: Dynamic max levels still available via LabDefinitionsExt");
			return false;
		}
	}
}
