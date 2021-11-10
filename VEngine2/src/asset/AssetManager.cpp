#include "AssetManager.h"
#include "handler/AssetHandler.h"
#include "Log.h"
#include "UUID.h"
#include "filesystem/VirtualFileSystem.h"
#include "filesystem/Path.h"
#include "AssetMetaDataRegistry.h" // for getAssetIDAndType()

AssetManager *AssetManager::s_instance = nullptr;
FileSystemWatcherHandle s_filesystemWatcherHandle = NULL_FILE_SYSTEM_WATCHER_HANDLE;

static void fileSystemWatcherCallback(const char *path, FileChangeType changeType, void *userData)
{
	if (changeType == FileChangeType::MODIFIED)
	{
		VirtualFileSystem &vfs = VirtualFileSystem::get();
		
		char metaFileName[VirtualFileSystem::k_maxPathLength];
		metaFileName[0] = '\0';
		strcat_s(metaFileName, path);
		strcat_s(metaFileName, ".meta");

		if (vfs.exists(metaFileName))
		{
			AssetID assetID;
			AssetType assetType;
			if (AssetMetaDataRegistry::getAssetIDAndType(metaFileName, &assetID, &assetType))
			{
				reinterpret_cast<AssetManager *>(userData)->reloadAsset(assetID, assetType);
			}
		}
	}
}

bool AssetManager::init(bool enableAutoReload)
{
	assert(!s_instance);
	s_instance = new AssetManager();

	if (enableAutoReload)
	{
		s_filesystemWatcherHandle = VirtualFileSystem::get().openFileSystemWatcher("/assets", fileSystemWatcherCallback, s_instance);
	}

	return true;
}

void AssetManager::shutdown()
{
	assert(s_instance);

	Log::info("Shutting down AssetManager");

	if (s_filesystemWatcherHandle != NULL_FILE_SYSTEM_WATCHER_HANDLE)
	{
		VirtualFileSystem::get().closeFileSystemWatcher(s_filesystemWatcherHandle);
		s_filesystemWatcherHandle = NULL_FILE_SYSTEM_WATCHER_HANDLE;
	}

	// unload reloaded assets
	if (!s_instance->m_reloadedAssetMap.empty())
	{
		Log::info("Unloading %u reloaded assets with internal references", (unsigned int)s_instance->m_reloadedAssetMap.size());

		s_instance->m_reloadedAssetMap.clear();
	}

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
	assert(eastl::string_view(path).starts_with("/assets/"));

	constexpr size_t assetsDirPathOffset = 8; // length of "/assets/" string without null terminator

	auto &vfs = VirtualFileSystem::get();

	// create AssetID
	AssetID resultAssetID = AssetID(path + assetsDirPathOffset);

	// create directory hierarchy
	size_t parentPathOffset = Path::getParentPath(path);
	char parentPath[IFileSystem::k_maxPathLength];
	memcpy(parentPath, path, parentPathOffset);
	parentPath[parentPathOffset] = '\0';
	vfs.createDirectoryHierarchy(parentPath);

	char assetTypeStr[AssetType::k_uuidStringSize];
	assetType.toString(assetTypeStr);

	// create meta file
	char metaFilePath[VirtualFileSystem::k_maxPathLength] = "\0";
	strcat_s(metaFilePath, path);
	strcat_s(metaFilePath, ".meta");
	if (auto fh = vfs.open(metaFilePath, FileMode::WRITE, true))
	{
		vfs.write(fh, strlen(resultAssetID.m_string), resultAssetID.m_string);
		vfs.write(fh, 1, "\n");

		vfs.write(fh, strlen(assetTypeStr), assetTypeStr);
		vfs.write(fh, 1, "\n");

		vfs.write(fh, strlen(sourcePath), sourcePath);
		vfs.write(fh, 1, "\n");

		vfs.close(fh);
	}

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
		auto it = m_assetMap.find(assetID);

		// guard against deleting new asset data when unloading old asset data after a reload
		if (it != m_assetMap.end() && it->second == assetData)
		{
			erased = m_assetMap.erase(assetID) >= 1;
		}
	}

	// destroy asset data 
	// (doesn't matter if the asset was not in the map, which might happen with old versions of reloaded assets)
	handler->destroyAsset(assetID, assetType, assetData);

	Log::info("Successfully unloaded asset \"%s\".", assetIDCopy.m_string);
}

void AssetManager::reloadAsset(const AssetID &assetID, const AssetType &assetType) noexcept
{
	AssetData *newAssetData = nullptr;
	AssetData *oldAssetData = nullptr;
	Asset<AssetData> prevReloadedAsset;
	{
		LOCK_HOLDER(m_assetMutex);

		auto assetIt = m_assetMap.find(assetID);

		// only allow reloading already loaded assets
		if (assetIt == m_assetMap.end())
		{
			return;
		}

		if (assetIt->second->getAssetType() != assetType)
		{
			// get string representations of AssetTypes
			char assetTypeStrArg[AssetType::k_uuidStringSize];
			assetType.toString(assetTypeStrArg);

			char assetTypeStrActual[AssetType::k_uuidStringSize];
			assetIt->second->getAssetType().toString(assetTypeStrActual);

			Log::warn("Tried to call AssetManager::reloadAsset() with  AssetID \"%s\" and AssetType \"%s\" but the asset has an AssetType of \"%s\"!", assetID.m_string, assetTypeStrArg, assetTypeStrActual);
			return;
		}

		// is this asset already queued for reload?
		auto reloadedAssetIt = m_reloadedAssetMap.find(assetID);
		if (reloadedAssetIt != m_reloadedAssetMap.end())
		{
			auto status = reloadedAssetIt->second->getAssetStatus();

			// no need to reload if the asset is still queued and has not yet begun loading
			if (status == AssetStatus::QUEUED_FOR_LOADING)
			{
				return;
			}

			// TODO: check and handle other possible states?

			// get the previous entry so that we can manually release it later without running into problems with (the lack of) reentrant locks
			prevReloadedAsset = reloadedAssetIt->second;
			m_reloadedAssetMap.erase(reloadedAssetIt);
		}

		// load from disk
		Log::info("Reloading asset \"%s\".", assetID.m_string);

		AssetHandler *handler = nullptr;

		// try to find asset handler
		{
			LOCK_HOLDER(m_assetHandlerMutex);

			auto handlerIt = m_assetHandlerMap.find(assetType);

			// failed to find handler
			if (handlerIt == m_assetHandlerMap.end())
			{
				Log::warn("Could not find asset handler for asset \"%s\"!", assetID.m_string);
				return;
			}

			handler = handlerIt->second;
		}

		// load asset
		newAssetData = handler->createAsset(assetID, assetType);
		{
			if (!newAssetData)
			{
				Log::warn("Failed to create asset \"%s\"!", assetID.m_string);
				return;
			}

			if (!handler->loadAssetData(newAssetData, (eastl::string("/assets/") + assetID.m_string).c_str()))
			{
				handler->destroyAsset(assetID, assetType, newAssetData);
				Log::warn("Failed to load asset \"%s\"!", assetID.m_string);
				return;
			}
		}

		// replace old asset in map with reloaded one
		oldAssetData = assetIt->second;
		assetIt->second = newAssetData;
	}

	prevReloadedAsset.release();
	
	// hold an internal reference to the reloaded asset
	{
		LOCK_HOLDER(m_assetMutex);
		m_reloadedAssetMap[assetID] = newAssetData;
		m_assetMap[assetID] = newAssetData;
	}

	// flag old asset as having a newer version available
	oldAssetData->setIsReloadedAssetAvailable(true);

	Log::info("Successfully reloaded asset \"%s\".", assetID.m_string);
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