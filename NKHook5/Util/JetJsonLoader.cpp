#include "JetJsonLoader.h"



#include "../Assets/AssetServer.h"



#include <Extensions/ExtensionManager.h>

#include <Files/ModArchive.h>

#include <Files/ZipBase.h>

#include <Logging/Logger.h>



#include <Util/ExtensionGlob.h>

#include <algorithm>
#include <cstring>
#include <filesystem>

#include <unordered_set>



using namespace Common::Extensions;

using namespace Common::Files;

using namespace Common::Logging::Logger;

using namespace NKHook5::Assets;
using namespace Common::Util;

namespace fs = std::filesystem;



namespace NKHook5::Util

{

	namespace

	{

		void AppendUniquePath(std::vector<std::string>& paths, std::unordered_set<std::string>& seen, const std::string& path)

		{

			if (path.empty() || !seen.insert(path).second)

				return;

			paths.push_back(path);

		}



		std::string NormalizeAssetJsonPath(const std::string& entry)
		{
			static constexpr const char* kModPrefix = "Mod/JSON/";
			if (entry.rfind(kModPrefix, 0) == 0)
				return std::string("Assets/JSON/") + entry.substr(strlen(kModPrefix));
			static constexpr const char* kJsonRoots[] = {
				"ScreenDefinitions/",
				"LabDefinitions/",
				"SpecialtyDefinitions/",
				"TowerDefinitions/",
				"StatusDefinitions/",
				"WeaponDefinitions/",
				"BloonDefinitions/",
			};
			for (const char* jsonRoot : kJsonRoots)
			{
				if (entry.rfind(jsonRoot, 0) == 0)
					return std::string("Assets/JSON/") + entry;
			}
			return entry;
		}

		void CollectFromArchive(const ZipBase& zip, const std::string& pathPrefix,

			const std::string& extensionSuffix, std::vector<std::string>& paths,

			std::unordered_set<std::string>& seen)

		{

			for (const std::string& entry : zip.GetEntries())

			{

				const std::string normalized = NormalizeAssetJsonPath(entry);

				if (normalized.rfind(pathPrefix, 0) != 0)

					continue;

				if (!extensionSuffix.empty() && normalized.size() >= extensionSuffix.size())

				{

					if (normalized.compare(normalized.size() - extensionSuffix.size(), extensionSuffix.size(), extensionSuffix) != 0)

						continue;

				}

				AppendUniquePath(paths, seen, normalized);

			}

		}

	}



	bool ReadVanillaJetBytes(const std::string& entryPath, std::vector<uint8_t>& out)

	{

		out.clear();

		ZipBase zip;

		if (!zip.Open("./Assets/BTD5.jet"))

			return false;



		zip.SetPassword("Q%_{6#Px]]");

		out = zip.ReadEntry(entryPath);

		zip.Close();

		return !out.empty();

	}



	bool ReadMergedJsonEntry(const std::string& entryPath, nlohmann::json& out)

	{

		std::vector<uint8_t> vanilla;

		ReadVanillaJetBytes(entryPath, vanilla);



		std::vector<uint8_t> merged = vanilla;

		if (AssetServer* server = AssetServer::GetServer())

		{

			if (auto served = server->ServeJSON(entryPath, vanilla))

				merged = served->GetData();

		}



		if (merged.empty())

			return false;



		try

		{

			out = nlohmann::json::parse(

				std::string(reinterpret_cast<const char*>(merged.data()), merged.size()),

				nullptr, true, true);

			return true;

		}

		catch (const std::exception&)

		{

			return false;

		}

	}



	std::vector<std::string> CollectAssetEntryPaths(const std::string& pathPrefix, const std::string& extensionSuffix)
	{
		std::vector<std::string> paths;
		std::unordered_set<std::string> seen;

		ZipBase jet;
		if (jet.Open("./Assets/BTD5.jet"))
		{
			jet.SetPassword("Q%_{6#Px]]");
			CollectFromArchive(jet, pathPrefix, extensionSuffix, paths, seen);
			jet.Close();
		}
		else
		{
			Print(LogLevel::WARNING, "JetJsonLoader: could not open ./Assets/BTD5.jet (cwd=%s)",
				fs::current_path().string().c_str());
		}

		const fs::path modsDir = fs::current_path() / "Mods";
		if (fs::exists(modsDir))
		{
			for (const auto& modEntry : fs::directory_iterator(modsDir))
			{
				if (!modEntry.is_regular_file())
					continue;
				try
				{
					ModArchive mod;
					if (!mod.OpenRead(modEntry.path()))
						continue;
					CollectFromArchive(mod, pathPrefix, extensionSuffix, paths, seen);
				}
				catch (const std::exception& ex)
				{
					Print(LogLevel::WARNING, "JetJsonLoader: failed to scan %s (%s)",
						modEntry.path().string().c_str(), ex.what());
				}
			}
		}

		if (AssetServer* server = AssetServer::GetServer())
		{
			for (const std::string& modPath : server->CollectEntryPaths(pathPrefix, extensionSuffix))
				AppendUniquePath(paths, seen, modPath);
		}

		std::sort(paths.begin(), paths.end());
		Print(LogLevel::INFO, "JetJsonLoader: discovered %zu path(s) under '%s' (*%s)",
			paths.size(), pathPrefix.c_str(), extensionSuffix.c_str());
		return paths;
	}



	std::vector<std::string> ReadMergedShopTypes(const std::string& screenDefPath, const char* itemsKey)

	{

		std::vector<std::string> types;

		nlohmann::json root;

		if (!ReadMergedJsonEntry(screenDefPath, root))

			return types;



		if (!root.contains(itemsKey))

			return types;



		for (const auto& item : root[itemsKey])

		{

			if (item.contains("Type"))

				types.push_back(item["Type"].get<std::string>());

		}

		return types;

	}



	void PreloadJsonExtension(JsonExtension& ext)

	{

		const std::string& target = ext.GetTarget();

		std::string pathPrefix = "Assets/JSON/";

		std::string extensionSuffix = ".json";



		const size_t jsonPos = target.find("/JSON/");

		if (jsonPos != std::string::npos)

		{

			const size_t afterJson = jsonPos + strlen("/JSON/");

			const size_t nextSlash = target.find('/', afterJson);

			if (nextSlash != std::string::npos)

			{

				pathPrefix += target.substr(afterJson, nextSlash - afterJson) + "/";

			}

		}



		const size_t dotPos = target.find_last_of('.');

		if (dotPos != std::string::npos && dotPos + 1 < target.size())

			extensionSuffix = target.substr(dotPos);



		const std::vector<std::string> entries = CollectAssetEntryPaths(pathPrefix, extensionSuffix);
		const bool isLabDefinitions = pathPrefix.find("LabDefinitions/") != std::string::npos;

		int loaded = 0;
		for (const std::string& entryPath : entries)
		{
			if (!MatchesExtensionTarget(target, entryPath))
				continue;

			nlohmann::json merged;
			if (!ReadMergedJsonEntry(entryPath, merged))
				continue;

			if (isLabDefinitions && !merged.contains("Name"))
			{
				const size_t slash = entryPath.find_last_of('/');
				std::string stem = slash == std::string::npos ? entryPath : entryPath.substr(slash + 1);
				const size_t dot = stem.find_last_of('.');
				if (dot != std::string::npos)
					stem = stem.substr(0, dot);
				merged["Name"] = "LOC_LAB_" + stem;
			}

			const std::string payload = merged.dump();
			ext.UseData(const_cast<char*>(payload.data()), payload.size());
			++loaded;
		}

		Print(LogLevel::INFO,
			"%s: preloaded %d merged asset(s) matching '%s' (from %zu candidates)",
			ext.GetName().c_str(), loaded, target.c_str(), entries.size());

	}

}

