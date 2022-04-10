#pragma once
#include "AssetHandler.h"

class AssetManager;

/// <summary>
/// AssetHandler implementation for skeleton assets.
/// </summary>
class SkeletonAssetHandler : public AssetHandler
{
public:
	/// <summary>
	/// Creates a SkeletonAssetHandler instance and registers it with the AssetManager.
	/// </summary>
	/// <param name="assetManager">The AssetManager to register this AssetHandler with.</param>
	static void init(AssetManager *assetManager) noexcept;
	static void shutdown() noexcept;
	AssetData *createEmptyAssetData(const AssetID &assetID, const AssetType &assetType) noexcept override;
	bool loadAssetData(AssetData *assetData, const char *path) noexcept override;
	void destroyAssetData(AssetData *assetData) noexcept override;

private:
};