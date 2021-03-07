#pragma once
#include <EASTL/hash_map.h>
#include <stdint.h>
#include "UUID.h"
#include "Asset.h"
#include "Utility/SpinLock.h"

class AssetData;
class AssetHandler;

class AssetManager
{
public:
	static bool init();
	static void shutdown();
	static AssetManager *get();

	template<typename T>
	Asset<T> getAsset(const AssetID &assetID) noexcept;
	Asset<AssetData> getAsset(const AssetID &assetID, const AssetType &assetType) noexcept;
	void unloadAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept;
	void registerAssetHandler(const AssetType &assetType, AssetHandler *handler) noexcept;
	void unregisterAssetHandler(const AssetType &assetType);

private:
	static AssetManager *s_instance;
	eastl::hash_map<AssetID, AssetData *, UUIDHash> m_assetMap;
	eastl::hash_map<AssetType, AssetHandler *, UUIDHash> m_assetHandlerMap;
	eastl::hash_map<AssetID, const char *, UUIDHash> m_assetIDToPath;
	SpinLock m_assetMutex;
	SpinLock m_assetHandlerMutex;
	SpinLock m_assetPathMutex;
};

template<typename T>
inline Asset<T> AssetManager::getAsset(const AssetID &assetID) noexcept
{
	auto asset = getAsset(assetID);
	return Asset<T>((T*)asset.get(), T::getAssetTypeStatic());
}
