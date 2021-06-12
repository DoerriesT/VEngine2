#include "MeshAssetHandler.h"
#include <assert.h>
#include "Log.h"
#include "utility/Utility.h"
#include "graphics/Renderer.h"
#include "MeshAsset.h"
#include "AssetManager.h"

static AssetManager *s_assetManager = nullptr;
static MeshAssetHandler s_meshAssetHandler;

void MeshAssetHandler::init(AssetManager *assetManager, Renderer *renderer) noexcept
{
	if (s_assetManager == nullptr)
	{
		s_assetManager = assetManager;
		s_meshAssetHandler.m_renderer = renderer;
		assetManager->registerAssetHandler(MeshAssetData::k_assetType, &s_meshAssetHandler);
	}
}

void MeshAssetHandler::shutdown() noexcept
{
	if (s_assetManager != nullptr)
	{
		s_assetManager->unregisterAssetHandler(MeshAssetData::k_assetType);
		s_meshAssetHandler = {};
		s_assetManager = nullptr;
	}
}

AssetData *MeshAssetHandler::createAsset(const AssetID &assetID, const AssetType &assetType) noexcept
{
	if (assetType != MeshAssetData::k_assetType)
	{
		Log::warn("MeshAssetHandler: Tried to call createAsset on a non-mesh asset!");
		return nullptr;
	}

	return new MeshAssetData(assetID);
}

bool MeshAssetHandler::loadAssetData(AssetData *assetData, const char *path) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != MeshAssetData::k_assetType)
	{
		Log::warn("MeshAssetHandler: Tried to call loadAssetData on a non-mesh asset!");
		return false;
	}

	assetData->setAssetStatus(AssetStatus::LOADING);

	// TODO: implement actual asset loading

	assetData->setAssetStatus(AssetStatus::READY);

	return true;
}

void MeshAssetHandler::destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != MeshAssetData::k_assetType)
	{
		Log::warn("MeshAssetHandler: Tried to call destroyAsset on a non-mesh asset!");
		return;
	}

	delete assetData;
}
