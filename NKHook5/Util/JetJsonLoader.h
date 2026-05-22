#pragma once



#include <Extensions/JsonExtension.h>

#include <nlohmann/json.hpp>

#include <string>

#include <vector>



namespace NKHook5::Util

{

	bool ReadVanillaJetBytes(const std::string& entryPath, std::vector<uint8_t>& out);

	bool ReadMergedJsonEntry(const std::string& entryPath, nlohmann::json& out);

	std::vector<std::string> CollectAssetEntryPaths(const std::string& pathPrefix, const std::string& extensionSuffix = ".json");

	std::vector<std::string> ReadMergedShopTypes(const std::string& screenDefPath, const char* itemsKey);

	void PreloadJsonExtension(Common::Extensions::JsonExtension& ext);

}


