#pragma once
#include <EASTL/hash_map.h>
#include <stdint.h>
#include "UUID.h"
#include "Asset.h"
#include "utility/SpinLock.h"

class AssetData;
class AssetHandler;
class AssetDatabase;

class AssetManager
{
public:
	static bool init();
	static void shutdown();
	static AssetManager *get();

	// assets
	AssetID createAsset(const AssetType &assetType, const char *path, const char *sourcePath) noexcept;

	template<typename T>
	Asset<T> getAsset(const AssetID &assetID) noexcept;
	AssetData *getAssetData(const AssetID &assetID, const AssetType &assetType) noexcept;
	void unloadAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept;
	
	// asset handlers
	
	void registerAssetHandler(const AssetType &assetType, AssetHandler *handler) noexcept;
	void unregisterAssetHandler(const AssetType &assetType);

private:
	static AssetManager *s_instance;
	eastl::hash_map<AssetID, AssetData *, StringIDHash> m_assetMap;
	eastl::hash_map<AssetType, AssetHandler *, UUIDHash> m_assetHandlerMap;
	SpinLock m_assetMutex;
	SpinLock m_assetHandlerMutex;

	explicit AssetManager() = default;
};

template<typename T>
inline Asset<T> AssetManager::getAsset(const AssetID &assetID) noexcept
{
	return getAssetData(assetID, T::k_assetType);
}
