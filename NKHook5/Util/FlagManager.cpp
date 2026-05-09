#include "FlagManager.h"

using namespace NKHook5::Util;

FlagManager::FlagManager() : nextSequentialId(1)
{
}

void FlagManager::Register(uint64_t numeric, const std::string& text)
{
	flags.emplace(numeric, text);
}

uint64_t FlagManager::Register(const std::string& text)
{
	// Sequential +1 registration - finds next available ID by incrementing
	// Skips existing bit flag IDs (1, 2, 4, 8, etc.) and already-registered IDs
	uint64_t id = nextSequentialId;
	while (!IsIDAvailable(id)) {
		id++;
	}
	nextSequentialId = id + 1;
	
	Register(id, text);
	printf("[FlagManager] Registered '%s' with sequential ID %llu\n", text.c_str(), id);
	return id;
}

uint64_t FlagManager::RegisterBitFlag(const std::string& text, int startBit)
{
	// Bit flag registration - finds next available bit flag starting from startBit
	// For bloons: should start from bit 20 (after Dreadbloon at bit 19)
	// For towers: should start from bit 59 (after GameDummy at bit 58)
	for (int i = startBit; i < 64; i++) {
		uint64_t flagValue = 1ull << i;
		if (IsIDAvailable(flagValue)) {
			Register(flagValue, text);
			printf("[FlagManager] Registered '%s' with bit flag 0x%llx (bit %d)\n", 
				text.c_str(), flagValue, i);
			return flagValue;
		}
	}
	
	// Bit slots exhausted - fallback to sequential +1
	printf("[FlagManager] WARNING: No bit slots for '%s', falling back to sequential ID\n", text.c_str());
	return Register(text);
}

bool FlagManager::IsIDAvailable(uint64_t id)
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
	for (int i = 0; i < 100; i++) {
		uint64_t numericId = static_cast<uint64_t>(1) << i;
		if (numericId == id)
		{
			return true;
		}
	}
	return false;
}

uint64_t FlagManager::GetFlag(const std::string& name)
{
	for (const auto& flagData : flags) {
		if (flagData.second == name) {
			return flagData.first;
		}
	}
	return 0;
}

std::string FlagManager::GetName(uint64_t flag)
{
	for (const auto& flagData : flags) {
		if (flagData.first == flag) {
			return flagData.second;
		}
	}
	return "INVALID";
}

const std::map<uint64_t, std::string>& FlagManager::GetAll()
{
	return this->flags;
}
