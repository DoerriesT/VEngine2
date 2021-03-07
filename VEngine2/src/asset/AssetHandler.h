#pragma once
#include "Asset.h"

class AssetHandler
{
public:
	virtual AssetData *createAsset(const AssetID &assetID, const AssetType &assetType) = 0;
	virtual bool loadAssetData(AssetData *assetData, const char *path) = 0;
	virtual void destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) = 0;
};