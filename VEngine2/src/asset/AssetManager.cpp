#include "AssetManager.h"
#include "handler/AssetHandler.h"
#include "Log.h"
#include "UUID.h"
#include <sstream>
#include <string>
#include "filesystem/VirtualFileSystem.h"
#include "filesystem/Path.h"

AssetManager *AssetManager::s_instance = nullptr;

bool AssetManager::init()
{
	assert(!s_instance);
	s_instance = new AssetManager();
	return true;
}

void AssetManager::shutdown()
{
	assert(s_instance);

	if (!s_instance->m_assetMap.empty())
	{
		Log::warn("AssetManager still had %u assets loaded when shutdown() was called!", (unsigned int)s_instance->m_assetMap.size());
	}

	// unload all remaining assets
	for (auto p : s_instance->m_assetMap)
	{
		auto assetID = p.first;
		auto assetType = p.second->getAssetType();
		auto assetData = p.second;

		auto handlerIt = s_instance->m_assetHandlerMap.find(assetType);

		// failed to find handler
		if (handlerIt == s_instance->m_assetHandlerMap.end())
		{
			Log::warn("Could not find asset handler for asset \"%s\"!", assetID.m_string);
			continue;
		}

		// destroy asset data
		AssetHandler *handler = handlerIt->second;
		handler->destroyAsset(assetID, assetType, assetData);
	}

	delete s_instance;
	s_instance = nullptr;
}

AssetManager *AssetManager::get()
{
	return s_instance;
}

AssetID AssetManager::createAsset(const AssetType &assetType, const char *path, const char *sourcePath) noexcept
{
	eastl::string_view sv(path);
	assert(sv.starts_with("/assets/"));

	constexpr size_t assetsDirPathOffset = 8; // length of "/assets/" string without null terminator

	// create AssetID
	AssetID resultAssetID = AssetID(path + assetsDirPathOffset);

	// create directory hierarchy
	size_t parentPathOffset = Path::getParentPath(path);
	char parentPath[IFileSystem::k_maxPathLength];
	memcpy(parentPath, path, parentPathOffset);
	parentPath[parentPathOffset] = '\0';
	VirtualFileSystem::get().createDirectoryHierarchy(parentPath);

	// create meta file
	std::stringstream metaFileData;
	metaFileData << resultAssetID.m_string << '\n';
	metaFileData << sourcePath << '\n';
	std::string str = metaFileData.str();

	VirtualFileSystem::get().writeFile((path + std::string(".meta")).c_str(), str.length(), str.data(), true);

	return resultAssetID;
}

AssetData *AssetManager::getAssetData(const AssetID &assetID, const AssetType &assetType) noexcept
{
	AssetData *assetData = nullptr;

	{
		// need to hold mutex so other treads dont try to create the same asset if it couldnt be found in the map
		LOCK_HOLDER(m_assetMutex);

		// try to find asset in map
		auto assetMapIt = m_assetMap.find(assetID);

		// found asset in map
		if (assetMapIt != m_assetMap.end())
		{
			return assetMapIt->second;
		}

		// couldnt find asset -> try to load from disk
		Log::info("Loading asset \"%s\".", assetID.m_string);

		AssetHandler *handler = nullptr;

		// try to find asset handler
		{
			LOCK_HOLDER(m_assetHandlerMutex);

			auto handlerIt = m_assetHandlerMap.find(assetType);

			// failed to find handler
			if (handlerIt == m_assetHandlerMap.end())
			{
				Log::warn("Could not find asset handler for asset \"%s\"!", assetID.m_string);
				return nullptr;
			}

			handler = handlerIt->second;
		}

		// load asset
		assetData = handler->createAsset(assetID, assetType);

		if (!assetData)
		{
			Log::warn("Failed to create asset \"%s\"!", assetID.m_string);
			return nullptr;
		}

		if (!handler->loadAssetData(assetData, (eastl::string("/assets/") + assetID.m_string).c_str()))
		{
			handler->destroyAsset(assetID, assetType, assetData);
			Log::warn("Failed to load asset \"%s\"!", assetID.m_string);
			return nullptr;
		}

		// store in map
		m_assetMap[assetID] = assetData;
	}

	Log::info("Successfully loaded asset \"%s\".", assetID.m_string);

	return assetData;
}

void AssetManager::unloadAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept
{
	// assetID is owned by the asset and when the asset gets deleted, so does the assetID,
	// which is why using the original assetID string is a bad idea.
	auto assetIDCopy = assetID;
	Log::info("Unloading asset \"%s\".", assetIDCopy.m_string);

	AssetHandler *handler = nullptr;

	// try to find asset handler
	{
		LOCK_HOLDER(m_assetHandlerMutex);

		auto handlerIt = m_assetHandlerMap.find(assetType);

		// failed to find handler
		if (handlerIt == m_assetHandlerMap.end())
		{
			Log::warn("Could not find asset handler for asset \"%s\"!", assetIDCopy.m_string);
			return;
		}

		handler = handlerIt->second;
	}

	// remove from map
	bool erased = false;
	{
		LOCK_HOLDER(m_assetMutex);
		erased = m_assetMap.erase(assetID) >= 1;
	}

	// destroy asset data
	if (erased)
	{
		handler->destroyAsset(assetID, assetType, assetData);
	}

	Log::info("Successfully unloaded asset \"%s\".", assetIDCopy.m_string);
}

void AssetManager::registerAssetHandler(const AssetType &assetType, AssetHandler *handler) noexcept
{
	LOCK_HOLDER(m_assetHandlerMutex);
	m_assetHandlerMap[assetType] = handler;
}

void AssetManager::unregisterAssetHandler(const AssetType &assetType)
{
	LOCK_HOLDER(m_assetHandlerMutex);
	m_assetHandlerMap.erase(assetType);
}