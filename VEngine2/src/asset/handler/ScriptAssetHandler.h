#pragma once
#include "AssetHandler.h"

class Renderer;
class AssetManager;

/// <summary>
/// AssetHandler implementation for script assets.
/// </summary>
class ScriptAssetHandler : public AssetHandler
{
public:
	/// <summary>
	/// Creates a ScriptAssetHandler instance and registers it with the AssetManager.
	/// </summary>
	/// <param name="assetManager">The AssetManager to register this AssetHandler with.</param>
	/// <returns></returns>
	static void init(AssetManager *assetManager) noexcept;
	static void shutdown() noexcept;
	AssetData *createEmptyAssetData(const AssetID &assetID, const AssetType &assetType) noexcept override;
	bool loadAssetData(AssetData *assetData, const char *path) noexcept override;
	void destroyAssetData(AssetData *assetData) noexcept override;

private:
};