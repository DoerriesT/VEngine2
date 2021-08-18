#pragma once
#include "AssetHandler.h"

class AnimationSystem;
class AssetManager;

/// <summary>
/// AssetHandler implementation for animation clip assets.
/// </summary>
class AnimationClipAssetHandler : public AssetHandler
{
public:
	/// <summary>
	/// Creates a AnimationClipAssetHandler instance and registers it with the AssetManager.
	/// </summary>
	/// <param name="assetManager">The AssetManager to register this AssetHandler with.</param>
	/// <param name="animationSystem">A pointer to the AnimationSystem. The AssetHandler uses this to create/destroy animation clips.</param>
	/// <returns></returns>
	static void init(AssetManager *assetManager, AnimationSystem *animationSystem) noexcept;
	static void shutdown() noexcept;
	AssetData *createAsset(const AssetID &assetID, const AssetType &assetType) noexcept override;
	bool loadAssetData(AssetData *assetData, const char *path) noexcept override;
	void destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept override;

private:
	AnimationSystem *m_animationSystem = nullptr;
};