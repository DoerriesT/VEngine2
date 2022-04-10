#pragma once
#include "asset/Asset.h"

class AssetHandler
{
public:
	virtual AssetData *createEmptyAssetData(const AssetID &assetID, const AssetType &assetType) noexcept = 0;
	virtual bool loadAssetData(AssetData *assetData, const char *path) noexcept = 0;
	virtual void destroyAssetData(AssetData *assetData) noexcept = 0;
};