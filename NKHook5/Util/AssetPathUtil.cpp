#include "AssetPathUtil.h"

#include <algorithm>
#include <cstring>

namespace NKHook5::Util
{
	std::string NormalizeArchivePath(std::string path)
	{
		std::replace(path.begin(), path.end(), '\\', '/');
		while (!path.empty() && path.front() == '/')
			path.erase(path.begin());
		return path;
	}

	std::string ToAssetsJsonPath(const std::string& path)
	{
		const std::string normalized = NormalizeArchivePath(path);
		static constexpr const char* kModPrefix = "Mod/JSON/";
		if (normalized.rfind(kModPrefix, 0) == 0)
			return std::string("Assets/JSON/") + normalized.substr(strlen(kModPrefix));
		return normalized;
	}
}
