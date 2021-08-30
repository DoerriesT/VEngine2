#include "AnimationClipAssetHandler.h"
#include <assert.h>
#include "Log.h"
#include "utility/Utility.h"
#include "animation/AnimationSystem.h"
#include "asset/AnimationClipAsset.h"
#include "asset/AssetManager.h"
#include "filesystem/VirtualFileSystem.h"

static AssetManager *s_assetManager = nullptr;
static AnimationClipAssetHandler s_animationClipAssetHandler;

void AnimationClipAssetHandler::init(AssetManager *assetManager, AnimationSystem *animationSystem) noexcept
{
	if (s_assetManager == nullptr)
	{
		s_assetManager = assetManager;
		s_animationClipAssetHandler.m_animationSystem = animationSystem;
		assetManager->registerAssetHandler(AnimationClipAssetData::k_assetType, &s_animationClipAssetHandler);
	}
}

void AnimationClipAssetHandler::shutdown() noexcept
{
	if (s_assetManager != nullptr)
	{
		s_assetManager->unregisterAssetHandler(AnimationClipAssetData::k_assetType);
		s_animationClipAssetHandler = {};
		s_assetManager = nullptr;
	}
}

AssetData *AnimationClipAssetHandler::createAsset(const AssetID &assetID, const AssetType &assetType) noexcept
{
	if (assetType != AnimationClipAssetData::k_assetType)
	{
		Log::warn("AnimationClipAssetHandler: Tried to call createAsset() on a non-animation clip asset!");
		return nullptr;
	}

	return new AnimationClipAssetData(assetID);
}

bool AnimationClipAssetHandler::loadAssetData(AssetData *assetData, const char *path) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != AnimationClipAssetData::k_assetType)
	{
		Log::warn("AnimationClipAssetHandler: Tried to call loadAssetData() on a non-animation clip asset!");
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
			auto handle = m_animationSystem->createAnimationClip(fileSize, fileData.data());
			static_cast<AnimationClipAssetData *>(assetData)->m_animationClipHandle = handle;
			success = handle != 0;
		}
		
		if (!success)
		{
			Log::warn("AnimationClipAssetHandler: Failed to load animation clip asset!");
			assetData->setAssetStatus(AssetStatus::ERROR);
			return false;
		}
	}

	assetData->setAssetStatus(AssetStatus::READY);

	return true;
}

void AnimationClipAssetHandler::destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != AnimationClipAssetData::k_assetType)
	{
		Log::warn("AnimationClipAssetHandler: Tried to call destroyAsset() on a non-animation clip asset!");
		return;
	}

	m_animationSystem->destroyAnimationClip(static_cast<AnimationClipAssetData *>(assetData)->m_animationClipHandle);

	delete assetData;
}
