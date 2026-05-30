#include "ModAssetSource.h"

#include "../../Util/AssetPathUtil.h"

#include <cstring>

using namespace Common;
using namespace Common::Files;
using namespace Common::Mod;
using namespace NKHook5;
using namespace NKHook5::Assets;
using namespace NKHook5::Util;
namespace fs = std::filesystem;

ModAssetSource::ModAssetSource(fs::path modPath) : AssetSource(modPath.stem().string())
{
	this->modArch = std::make_shared<ModArchive>();
	if (!this->modArch->OpenRead(modPath)) {
		throw std::exception((std::string("Failed to read mod file: ") + modPath.string()).c_str());
	}
}

const std::shared_ptr<ModArchive>& ModAssetSource::GetModArch()
{
	return this->modArch;
}

bool ModAssetSource::Has(fs::path assetPath)
{
	const std::string primary = NormalizeArchivePath(assetPath.string());
	if (this->modArch->HasEntry(primary))
		return true;
	if (primary.rfind("Assets/JSON/", 0) == 0)
	{
		const std::string jsonRel = primary.substr(strlen("Assets/JSON/"));
		return this->modArch->HasEntry(jsonRel)
			|| this->modArch->HasEntry(std::string("Mod/JSON/") + jsonRel);
	}
	return false;
}

std::shared_ptr<Asset> ModAssetSource::Find(fs::path assetPath)
{
	const std::string primary = NormalizeArchivePath(assetPath.string());
	std::vector<uint8_t> entryData = this->modArch->ReadEntry(primary);
	if (entryData.empty() && primary.rfind("Assets/JSON/", 0) == 0)
	{
		const std::string jsonRel = primary.substr(strlen("Assets/JSON/"));
		entryData = this->modArch->ReadEntry(jsonRel);
		if (entryData.empty())
			entryData = this->modArch->ReadEntry(std::string("Mod/JSON/") + jsonRel);
	}

	if (entryData.empty())
		return nullptr;
	return std::make_shared<Asset>(fs::path(primary), entryData);
}

std::shared_ptr<Asset> ModAssetSource::FindRel(AssetBin bin, fs::path relativePath)
{
	std::string binDir = ToString(bin);
	fs::path lookupPath = "Assets/";
	lookupPath /= binDir;
	lookupPath /= relativePath;
	return this->Find(lookupPath);
}

std::shared_ptr<Asset> ModAssetSource::FindInBin(AssetBin bin, std::string filename)
{
	std::string binDir = ToString(bin);
	for (const auto& entry : this->modArch->GetEntries()) {
		fs::path entryPath = entry.substr(sizeof("Assets/") - 1);
		if (entryPath.string().find(binDir) == 0) {
			if (entryPath.filename() == filename || entryPath.stem() == filename) {
				return this->Find(entry);
			}
		}
	}
	return nullptr;
}
