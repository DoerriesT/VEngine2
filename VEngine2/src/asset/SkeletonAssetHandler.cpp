#include "SkeletonAssetHandler.h"
#include <assert.h>
#include "Log.h"
#include "utility/Utility.h"
#include "animation/AnimationSystem.h"
#include "SkeletonAsset.h"
#include "AssetManager.h"
#include <fstream>

static AssetManager *s_assetManager = nullptr;
static SkeletonAssetHandler s_skeletonAssetHandler;

void SkeletonAssetHandler::init(AssetManager *assetManager, AnimationSystem *animationSystem) noexcept
{
	if (s_assetManager == nullptr)
	{
		s_assetManager = assetManager;
		s_skeletonAssetHandler.m_animationSystem = animationSystem;
		assetManager->registerAssetHandler(SkeletonAssetData::k_assetType, &s_skeletonAssetHandler);
	}
}

void SkeletonAssetHandler::shutdown() noexcept
{
	if (s_assetManager != nullptr)
	{
		s_assetManager->unregisterAssetHandler(SkeletonAssetData::k_assetType);
		s_skeletonAssetHandler = {};
		s_assetManager = nullptr;
	}
}

AssetData *SkeletonAssetHandler::createAsset(const AssetID &assetID, const AssetType &assetType) noexcept
{
	if (assetType != SkeletonAssetData::k_assetType)
	{
		Log::warn("SkeletonAssetHandler: Tried to call createAsset() on a non-skeleton asset!");
		return nullptr;
	}

	return new SkeletonAssetData(assetID);
}

bool SkeletonAssetHandler::loadAssetData(AssetData *assetData, const char *path) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != SkeletonAssetData::k_assetType)
	{
		Log::warn("SkeletonAssetHandler: Tried to call loadAssetData() on a non-skeleton asset!");
		return false;
	}

	assetData->setAssetStatus(AssetStatus::LOADING);

	// load asset
	{
		// load file
		size_t fileSize = 0;
		char *fileData = util::readBinaryFile(path, &fileSize);

		AnimationSkeletonHandle skeletonHandle = m_animationSystem->createSkeleton(fileSize, fileData);

		delete[] fileData;

		if (!skeletonHandle)
		{
			Log::warn("SkeletonAssetHandler: Failed to load skeleton asset!");
			assetData->setAssetStatus(AssetStatus::ERROR);
			return false;
		}

		static_cast<SkeletonAssetData *>(assetData)->m_skeletonHandle = skeletonHandle;
	}

	assetData->setAssetStatus(AssetStatus::READY);

	return true;
}

void SkeletonAssetHandler::destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != SkeletonAssetData::k_assetType)
	{
		Log::warn("SkeletonAssetHandler: Tried to call destroyAsset() on a non-skeleton asset!");
		return;
	}

	m_animationSystem->destroySkeleton(static_cast<SkeletonAssetData *>(assetData)->m_skeletonHandle);

	delete assetData;
}
