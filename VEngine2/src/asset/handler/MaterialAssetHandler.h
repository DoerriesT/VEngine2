#pragma once
#include "AssetHandler.h"

class Renderer;
class Physics;
class AssetManager;

/// <summary>
/// AssetHandler implementation for material assets.
/// </summary>
class MaterialAssetHandler : public AssetHandler
{
public:
	/// <summary>
	/// Creates a MeshAssetHandler instance and registers it with the AssetManager.
	/// </summary>
	/// <param name="assetManager">The AssetManager to register this AssetHandler with.</param>
	/// <param name="renderer">A pointer to the Renderer. The AssetHandler uses this to create/destroy meshes.</param>
	/// <returns></returns>
	static void init(AssetManager *assetManager, Renderer *renderer) noexcept;
	static void shutdown() noexcept;
	AssetData *createAsset(const AssetID &assetID, const AssetType &assetType) noexcept override;
	bool loadAssetData(AssetData *assetData, const char *path) noexcept override;
	void destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept override;

private:
	Renderer *m_renderer = nullptr;
};