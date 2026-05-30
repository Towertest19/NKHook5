#pragma once

#include <string>

namespace NKHook5::Util
{
	std::string NormalizeArchivePath(std::string path);
	std::string ToAssetsJsonPath(const std::string& path);
	std::string ToDefinitionAssetPath(const std::string& path);
}
