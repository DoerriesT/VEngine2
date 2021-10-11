#pragma once
#include <EASTL/hash_map.h>
#include <EASTL/string_hash_map.h>
#include <EASTL/vector.h>
#include "Asset.h"
#include "utility/SpinLock.h"
#include "filesystem/IFileSystem.h"

class AssetMetaDataRegistry
{
	friend void fileSystemWatcherCallback(const char *path, FileChangeType changeType, void *userData);
public:
	static AssetMetaDataRegistry *get();
	static bool getAssetIDAndType(const char *metaFilePath, AssetID *assetID, AssetType *assetType) noexcept;

	bool init() noexcept;
	void shutdown() noexcept;
	bool getAssetIDs(const AssetType &assetType, size_t *count, AssetID *resultArray) noexcept;
	bool getAssetIDAt(const AssetType &assetType, size_t idx, AssetID *resultAssetID) noexcept;
	bool getAssetType(const AssetID &assetID, AssetType *resultAssetType) noexcept;
	bool isRegistered(const AssetID &assetID) noexcept;

private:
	mutable SpinLock m_typeToIDsMutex;
	mutable SpinLock m_idToTypeMutex;
	eastl::hash_map<AssetType, eastl::vector<AssetID>, UUIDHash> m_assetTypeToIDs;
	eastl::hash_map<AssetID, AssetType, StringIDHash> m_assetIDToType;
	eastl::string_hash_map<AssetID> m_pathToAssetID;
	FileSystemWatcherHandle m_watcherHandle = {};
};