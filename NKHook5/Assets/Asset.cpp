#include "Asset.h"

#include <algorithm>

using namespace NKHook5;
using namespace NKHook5::Assets;

namespace
{
	fs::path NormalizeAssetPath(fs::path assetPath)
	{
		std::string normalized = assetPath.string();
		std::replace(normalized.begin(), normalized.end(), '\\', '/');
		return fs::path(normalized);
	}
}

Asset::Asset(fs::path assetPath)
{
	assetPath = NormalizeAssetPath(assetPath);
	this->assetBin = FromPath(assetPath);
	this->assetPath = assetPath.string()
		.substr(sizeof("Assets/"))
		.substr(ToString(this->assetBin).size());
}

Asset::Asset(fs::path assetPath, std::vector<uint8_t> assetData)
{
	assetPath = NormalizeAssetPath(assetPath);
	this->assetBin = FromPath(assetPath);
	this->assetPath = assetPath.string()
		.substr(sizeof("Assets/"))
		.substr(ToString(this->assetBin).size());
	this->assetData = assetData;
}

const AssetBin& Asset::GetBin()
{
	return this->assetBin;
}

const fs::path& Asset::GetRelPath()
{
	return this->assetPath;
}

fs::path Asset::GetFullPath()
{
	fs::path result = "Assets";
	result /= ToString(this->GetBin());
	result /= this->GetRelPath();
	return result;
}

std::vector<uint8_t> Asset::GetData()
{
	return this->assetData;
}
