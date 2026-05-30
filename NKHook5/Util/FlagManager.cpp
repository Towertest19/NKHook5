#include "FlagManager.h"

#include <Logging/Logger.h>

using namespace NKHook5::Util;
using namespace Common::Logging::Logger;

FlagManager::FlagManager() : nextSequentialId(1)
{
}

void FlagManager::Register(uint64_t numeric, const std::string& text)
{
	flags.emplace(numeric, text);
}

uint64_t FlagManager::Register(const std::string& text)
{
	if (const uint64_t existing = GetFlag(text))
		return existing;

	// Numeric +1 registration - finds next available ID by incrementing
	// Skips existing bit flag IDs (1, 2, 4, 8, etc.) and already-registered IDs
	uint64_t id = nextSequentialId;
	while (!IsIDAvailable(id)) {
		id++;
	}
	nextSequentialId = id + 1;

	Register(id, text);
	Print(LogLevel::DEBUG, "FlagManager: registered '%s' with ID %llu", text.c_str(), id);
	return id;
}

uint64_t FlagManager::RegisterBitFlag(const std::string& text, int startBit)
{
	if (const uint64_t existing = GetFlag(text))
		return existing;

	// Bit flag registration - finds next available bit flag starting from startBit
	// For bloons: should start from bit 20 (after Dreadbloon at bit 19)
	// For towers: should start from bit 59 (bit 58 is GameDummy, an internal vanilla helper)
	for (int i = startBit; i < 64; i++) {
		uint64_t flagValue = 1ull << i;
		if (IsIDAvailable(flagValue)) {
			Register(flagValue, text);
			Print(LogLevel::DEBUG, "FlagManager: registered '%s' with bit flag 0x%llx (bit %d)",
				text.c_str(), flagValue, i);
			return flagValue;
		}
	}

	// Bit slots exhausted - fallback to numeric +1
	Print(LogLevel::DEBUG, "FlagManager: no bit slots for '%s', falling back to ID lookup", text.c_str());
	return Register(text);
}

bool FlagManager::IsIDAvailable(uint64_t id) const
{
	if (id == 0) {
		return false;
	}
	for (const auto& flagData : flags) {
		if (flagData.first == id) {
			return false;
		}
	}
	return true;
}

bool FlagManager::IsVanilla(uint64_t id)
{
	return IsBitFlag(id);
}

bool FlagManager::IsBaseTower(uint64_t id)
{
	return id >= (1ull << 2ull) && id <= (1ull << 24ull) && IsBitFlag(id);
}

bool FlagManager::IsCustomBitFlag(uint64_t id)
{
	return id >= (1ull << 59ull) && IsBitFlag(id);
}

bool FlagManager::IsCustomFallbackId(uint64_t id)
{
	return id > 0 && id < (1ull << 59ull) && !IsBitFlag(id);
}

uint64_t FlagManager::GetFlag(const std::string& name) const
{
	for (const auto& flagData : flags) {
		if (flagData.second == name) {
			return flagData.first;
		}
	}
	return 0;
}

std::string FlagManager::GetName(uint64_t flag) const
{
	for (const auto& flagData : flags) {
		if (flagData.first == flag) {
			return flagData.second;
		}
	}
	return "INVALID";
}

const std::map<uint64_t, std::string>& FlagManager::GetAll() const
{
	return this->flags;
}
