#pragma once
#include "Asset.h"

class AssetHandler
{
public:
	virtual AssetData *createAsset(const AssetID &assetID, const AssetType &assetType) noexcept = 0;
	virtual bool loadAssetData(AssetData *assetData, const char *path) noexcept = 0;
	virtual void destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept = 0;
};