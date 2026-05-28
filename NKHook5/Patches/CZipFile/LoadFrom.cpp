#include "LoadFrom.h"

#include "../../Assets/AssetServer.h"
#include "../../Extensions/LabDefinitions/LabDefinitionsExt.h"
#include "../../Extensions/SpecialtyDefinitions/SpecialtyDefinitionsExt.h"
#include "../../Extensions/TowerInfo/TowerInfoExt.h"
#include "../RuntimeHooks.h"
#include "../../Util/FlagManager.h"

#include <Extensions/ExtensionManager.h>
#include <nlohmann/json.hpp>
#include <Files/File.h>
#include <Logging/Logger.h>

#include <filesystem>

extern NKHook5::Util::FlagManager g_towerFlags;

namespace NKHook5::Patches::CZipFile
{
	using namespace Common::Extensions;
	using namespace Common::Files;
	using namespace Common::Logging;
	using namespace Common::Logging::Logger;
	using namespace NKHook5;
	using namespace NKHook5::Extensions::LabDefinitions;
	using namespace NKHook5::Extensions::SpecialtyDefinitions;
	using namespace NKHook5::Extensions::TowerInfo;
	using namespace NKHook5::Patches::RuntimeHooks;
	using namespace NKHook5::Signatures;
	using namespace NKHook5::Assets;
	namespace fs = std::filesystem;

	namespace
	{
		void DeleteFileIfExists(const nfw::string& path)
		{
			if (path.empty())
				return;
			const fs::path tmpPath(std::string(path.c_str(), path.length()));
			if (tmpPath.extension() != ".tmp")
				return;
			std::error_code ec;
			fs::remove(tmpPath, ec);
		}
	}

	uint64_t o_func;
	Classes::CUnzippedFile* LoadFrom::cb_hook(const nfw::string& assetPath, void* param_2, nfw::string& archivePassword) {
		//Get the extensions for the file
		std::vector<Extension*> extsForFile = ExtensionManager::GetByTarget(std::string(assetPath));

		Classes::CUnzippedFile* pAsset = nullptr;

		//Get the vanilla asset
		auto ofn = std::bit_cast<decltype(&LoadFrom::cb_hook)>(reinterpret_cast<void*>(o_func));
		pAsset = (this->*ofn)(assetPath, param_2, archivePassword);

		//Store the vanilla data into a vector
		std::vector<uint8_t> vanillaData;

		if (pAsset) {
			vanillaData = std::vector<uint8_t>(pAsset->fileSize);
			memcpy_s(vanillaData.data(), vanillaData.size(), pAsset->fileContent, pAsset->fileSize);
		}
		else {
			if (fs::exists(assetPath)) {
				File vanillaFile;
				vanillaFile.OpenRead(assetPath);
				vanillaData = vanillaFile.ReadBytes();
				vanillaFile.Close();
			}
			else {
#ifdef _DEBUG
				Print(LogLevel::ERR, "Failed to find asset: %s", assetPath.c_str());
#endif
			}
		}

		//Get the AssetServer
		AssetServer* server = AssetServer::GetServer();
		const std::string assetPathStr(assetPath.c_str(), assetPath.length());
		const bool isSpecialtyDefinition =
			assetPathStr.rfind("Assets/JSON/SpecialtyDefinitions/", 0) == 0;

		//Serve the asset (vanilla + mod .nkh merge)
		std::shared_ptr<Asset> servedAsset = server->Serve(assetPath, vanillaData);

		// Extensions must see merged JSON (mods), not jet-only vanilla.
		std::vector<uint8_t> extensionData = vanillaData;
		if (servedAsset != nullptr)
			extensionData = servedAsset->GetData();

		if (isSpecialtyDefinition)
			SpecialtyDefinitionsExt::PatchMergedAssetBytes(extensionData);

		const bool isMainMenuBuildingsJson =
			assetPathStr.rfind("Assets/JSON/ScreenDefinitions/MainMenu/Buildings", 0) == 0 &&
			(assetPathStr.ends_with("Buildings.json") || assetPathStr.ends_with("BuildingsNoSocial.json"));
		if (isMainMenuBuildingsJson && !extensionData.empty())
			TowerInfoExt::TryAugmentBuildingsBytes(extensionData, g_towerFlags);

		const bool isLabShopJson =
			assetPathStr == "Assets/JSON/ScreenDefinitions/MainMenu/LabShop.json";
		if (isLabShopJson && !extensionData.empty())
		{
			try
			{
				nlohmann::json labShop = nlohmann::json::parse(
					std::string(reinterpret_cast<const char*>(extensionData.data()), extensionData.size()),
					nullptr, true, true);
				if (auto* labExt = ExtensionManager::Get<LabDefinitionsExt>())
				{
					if (labExt->AugmentLabShopJson(labShop))
					{
						const std::string patched = labShop.dump();
						extensionData.assign(patched.begin(), patched.end());
					}
				}
			}
			catch (const std::exception&)
			{
			}
		}

		const bool isSpecialtyShopJson =
			assetPathStr == "Assets/JSON/ScreenDefinitions/MainMenu/SpecialtyShop.json";
		if (isSpecialtyShopJson && !extensionData.empty())
		{
			try
			{
				nlohmann::json specialtyShop = nlohmann::json::parse(
					std::string(reinterpret_cast<const char*>(extensionData.data()), extensionData.size()),
					nullptr, true, true);
				if (auto* specExt = ExtensionManager::Get<SpecialtyDefinitionsExt>())
				{
					if (specExt->AugmentSpecialtyShopJson(specialtyShop))
					{
						const std::string patched = specialtyShop.dump();
						extensionData.assign(patched.begin(), patched.end());
					}
				}
			}
			catch (const std::exception&)
			{
			}
		}

		if (!extensionData.empty()) {
			for (Extension* ext : extsForFile) {
				if (!ext->IsCustomDocument())
					ext->UseData(extensionData.data(), extensionData.size());
			}
		}

		OnMergedAssetLoaded(assetPathStr);
		DeleteFileIfExists(assetPath);

		//If there is an asset to serve
		if (servedAsset != nullptr) {
			std::vector<uint8_t> servedData = extensionData.empty() ? servedAsset->GetData() : extensionData;
			if (isSpecialtyDefinition)
				SpecialtyDefinitionsExt::PatchMergedAssetBytes(servedData);

			if (pAsset) {
				//Create a copy
				void* contentCopy = malloc(servedData.size());
				memcpy_s(
					contentCopy,
					servedData.size(),
					servedData.data(),
					servedData.size()
				);
				if (pAsset->fileContent) {
					free(pAsset->fileContent);
				}
				//Place the new pointer and size into the asset structure
				pAsset->fileContent = contentCopy;
				pAsset->fileSize = servedData.size();
			}
			else {
				auto patchedAsset = std::make_shared<Asset>(fs::path(assetPathStr), servedData);
				pAsset = new Classes::CUnzippedFile(patchedAsset);
			}
		}

		return pAsset;
	}

	auto LoadFrom::Apply() -> bool
	{
		const void* address = Signatures::GetAddressOf(Sigs::CZipFile_LoadFrom);
		if (address)
		{
			auto* detour = new PLH::x86Detour((const uint64_t)address, std::bit_cast<size_t>(&LoadFrom::cb_hook), &o_func);
			if (detour->hook())
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
}