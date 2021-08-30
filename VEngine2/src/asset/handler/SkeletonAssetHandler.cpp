#include "SkeletonAssetHandler.h"
#include <assert.h>
#include "Log.h"
#include "utility/Utility.h"
#include "animation/AnimationSystem.h"
#include "asset/SkeletonAsset.h"
#include "asset/AssetManager.h"
#include "filesystem/VirtualFileSystem.h"

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
		auto fileSize = VirtualFileSystem::get().size(path);
		eastl::vector<char> fileData(fileSize);

		bool success = false;

		if (VirtualFileSystem::get().readFile(path, fileSize, fileData.data(), true))
		{
			auto handle = m_animationSystem->createSkeleton(fileSize, fileData.data());
			static_cast<SkeletonAssetData *>(assetData)->m_skeletonHandle = handle;
			success = handle != 0;
		}
		
		if (!success)
		{
			Log::warn("SkeletonAssetHandler: Failed to load skeleton asset!");
			assetData->setAssetStatus(AssetStatus::ERROR);
			return false;
		}
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
