#pragma once
#include "AssetHandler.h"

class Renderer;
class Physics;
class AssetManager;

/// <summary>
/// AssetHandler implementation for mesh assets.
/// </summary>
class MeshAssetHandler : public AssetHandler
{
public:
	/// <summary>
	/// Creates a MeshAssetHandler instance and registers it with the AssetManager.
	/// </summary>
	/// <param name="assetManager">The AssetManager to register this AssetHandler with.</param>
	/// <param name="renderer">A pointer to the Renderer. The AssetHandler uses this to create/destroy meshes.</param>
	/// <param name="physics">A pointer to the Physics system. The AssetHandler uses this to create/destroy physics meshes.</param>
	/// <returns></returns>
	static void init(AssetManager *assetManager, Renderer *renderer, Physics *physics) noexcept;
	static void shutdown() noexcept;
	AssetData *createAsset(const AssetID &assetID, const AssetType &assetType) noexcept override;
	bool loadAssetData(AssetData *assetData, const char *path) noexcept override;
	void destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept override;

private:
	Renderer *m_renderer = nullptr;
	Physics *m_physics = nullptr;
};