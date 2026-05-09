#pragma once

#include "NewFramework.h"

namespace NKHook5::Util {
	class FlagManager {
		std::map<uint64_t, std::string> flags;
		uint64_t nextSequentialId; // Per-manager counter for +1 lookup
	public:
		static bool IsVanilla(uint64_t id);
		// Check if ID is a bit flag (proper targeting) vs sequential ID
		static bool IsBitFlag(uint64_t id) { return (id & (id - 1)) == 0 && id != 0; }

		FlagManager();
		//Registers at a specific ID
		void Register(uint64_t numeric, const std::string& text);
		//Registers at next available sequential ID (+1 lookup, skips taken IDs)
		uint64_t Register(const std::string& text);
		//Registers at next available bit flag starting from startBit
		// startBit=20 for bloons (after Dreadbloon), 59 for towers (after GameDummy)
		uint64_t RegisterBitFlag(const std::string& text, int startBit = 59);
		//Slowly check if the id is available
		bool IsIDAvailable(uint64_t id);
		uint64_t GetFlag(const std::string& name);
		std::string GetName(uint64_t flag);
		const std::map<uint64_t, std::string>& GetAll();
	};
}
