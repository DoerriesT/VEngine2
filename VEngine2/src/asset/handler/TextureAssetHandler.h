#pragma once
#include "AssetHandler.h"

class Renderer;
class AssetManager;

/// <summary>
/// AssetHandler implementation for texture assets.
/// </summary>
class TextureAssetHandler : public AssetHandler
{
public:
	/// <summary>
	/// Creates a TextureAssetHandler instance and registers it with the AssetManager.
	/// </summary>
	/// <param name="assetManager">The AssetManager to register this AssetHandler with.</param>
	/// <param name="renderer">A pointer to the Renderer. The AssetHandler uses this to create/destroy textures.</param>
	/// <returns></returns>
	static void init(AssetManager *assetManager, Renderer *renderer) noexcept;
	static void shutdown() noexcept;
	AssetData *createEmptyAssetData(const AssetID &assetID, const AssetType &assetType) noexcept override;
	bool loadAssetData(AssetData *assetData, const char *path) noexcept override;
	void destroyAssetData(AssetData *assetData) noexcept override;

private:
	Renderer *m_renderer = nullptr;
};