#include "Utils.h"

#include "Logging/Logger.h"

#include <Windows.h>
#include <Psapi.h>

#include <magic_enum/magic_enum.hpp>

#include <libhat/Scanner.hpp>

#include <cassert>
#include <vector>

#define INRANGE(x,a,b)	(x >= a && x <= b) 
#define getBits( x )	(INRANGE((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xa) : (INRANGE(x,'0','9') ? x - '0' : 0))
#define getByte( x )	(getBits(x[0]) << 4 | getBits(x[1]))


using namespace NKHook5;
using namespace Common::Logging::Logger;

size_t Utils::GetModuleBase()
{
	return (size_t)GetModuleHandle(nullptr);
}
size_t Utils::GetBaseModuleSize()
{
	MODULEINFO info;
	GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &info, sizeof(info));
	return info.SizeOfImage;
}
size_t Utils::GetBaseModuleEnd()
{
	return GetModuleBase() + GetBaseModuleSize();
}
size_t Utils::FindPattern(std::string_view pattern)
{
	auto sig = hat::parse_signature(pattern);
	if (sig.has_value()) {
		const auto result = hat::find_pattern(sig.value(), ".text");
		if (result.has_result()) {
			auto addr = reinterpret_cast<uintptr_t>(result.get());
			// Validate the address is within the executable module bounds
			auto base = GetModuleBase();
			auto end = GetBaseModuleEnd();
			if (addr >= base && addr < end) {
				if (!IsAddressExecutable(addr))
				{
					Print(LogLevel::ERR, "Sig '%s' found non-executable address %p; rejecting match", 
						pattern.data(), (void*)addr);
					return 0;
				}
				return addr;
			}
			Print(LogLevel::ERR, "Sig '%s' found address %p outside module range [%p - %p]", 
				pattern.data(), (void*)addr, (void*)base, (void*)end);
			return 0;
		}
		return NULL;
	} else {
		Print(LogLevel::ERR, "Sig failed with '%s' (%s)", pattern.data(), magic_enum::enum_name(sig.error()).data());
	}
	return 0;
}

size_t Utils::FindPattern(size_t rangeStart, size_t rangeEnd, std::string_view pattern) {
	auto sig = hat::parse_signature(pattern);
	if (sig.has_value()) {
		const auto result = hat::find_pattern(
				reinterpret_cast<const std::byte *>(rangeStart),
				reinterpret_cast<const std::byte *>(rangeEnd),
				sig.value());
		if (result.has_result()) {
			auto addr = reinterpret_cast<uintptr_t>(result.get());
			// Validate the address is within the specified range
			if (addr >= rangeStart && addr < rangeEnd) {
				if (!IsAddressExecutable(addr))
				{
					Print(LogLevel::ERR, "Sig '%s' found non-executable address %p; rejecting match", 
						pattern.data(), (void*)addr);
					return 0;
				}
				return addr;
			}
			Print(LogLevel::ERR, "Sig '%s' found address %p outside specified range [%p - %p]",
				pattern.data(), (void*)addr, (void*)rangeStart, (void*)rangeEnd);
			return 0;
		}
		return NULL;
	} else {
		Print(LogLevel::ERR, "Sig failed with '%s' (%s)", pattern.data(), magic_enum::enum_name(sig.error()).data());
	}
	return 0;
}

bool Utils::IsAddressExecutable(size_t address)
{
	if (address == 0) {
		return false;
	}

	// Check if address is within our module first
	size_t base = GetModuleBase();
	size_t end = GetBaseModuleEnd();
	if (address < base || address >= end) {
		return false;
	}

	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQuery((LPCVOID)address, &mbi, sizeof(mbi))) {
		return (mbi.State == MEM_COMMIT) && 
		       (mbi.Protect == PAGE_EXECUTE || 
		        mbi.Protect == PAGE_EXECUTE_READ || 
		        mbi.Protect == PAGE_EXECUTE_READWRITE ||
		        mbi.Protect == PAGE_EXECUTE_WRITECOPY);
	}
	return false;
}

std::string Utils::GetTypeName(void* object)
{
	void* vtable = *(void**)object;
	void** metaPtr = (void**)((size_t)vtable-(size_t)sizeof(void*));
	void** typeDesc = (void**)((size_t)*metaPtr+(sizeof(void*)*3));
	char* name = (char*)((size_t)*typeDesc+(sizeof(void*)*3));
	return { name };
}