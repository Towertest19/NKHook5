#pragma once

#include <string>
#include <polyhook2/Detour/x86Detour.hpp>
#include <polyhook2/ZydisDisassembler.hpp>
#include "../Utils.h"
#include "../../Common/Logging/Logger.h"

namespace NKHook5::Patches
{
	class IPatch
	{
		static inline PLH::ZydisDisassembler* dis = new PLH::ZydisDisassembler(PLH::Mode::x86);
		std::string name;
	protected:
		bool ValidateAddress(uintptr_t address) {
			if (address == 0) {
				Common::Logging::Logger::Print(Common::Logging::Logger::LogLevel::ERR, 
					"Patch '%s': Address is null", name.c_str());
				return false;
			}
			
			if (!Utils::IsAddressExecutable(address)) {
				Common::Logging::Logger::Print(Common::Logging::Logger::LogLevel::ERR, 
					"Patch '%s': Address %p is not executable memory", name.c_str(), (void*)address);
				return false;
			}
			
			return true;
		}
	public:
		IPatch(std::string name);
		auto GetName() -> std::string;
		auto GetDis() -> PLH::ZydisDisassembler&;
		void WriteBytes(uintptr_t address, const char* bytes, size_t len);
		virtual auto Apply() -> bool;
	};
} // namespace NKHook5::Patches