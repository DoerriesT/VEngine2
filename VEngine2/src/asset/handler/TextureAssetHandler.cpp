#include "TextureAssetHandler.h"
#include <assert.h>
#include "Log.h"
#include "utility/Utility.h"
#include "graphics/Renderer.h"
#include "asset/TextureAsset.h"
#include "asset/AssetManager.h"
#include "filesystem/VirtualFileSystem.h"

static AssetManager *s_assetManager = nullptr;
static TextureAssetHandler s_textureAssetHandler;

void TextureAssetHandler::init(AssetManager *assetManager, Renderer *renderer) noexcept
{
	if (s_assetManager == nullptr)
	{
		s_assetManager = assetManager;
		s_textureAssetHandler.m_renderer = renderer;
		assetManager->registerAssetHandler(TextureAssetData::k_assetType, &s_textureAssetHandler);
	}
}

void TextureAssetHandler::shutdown() noexcept
{
	if (s_assetManager != nullptr)
	{
		s_assetManager->unregisterAssetHandler(TextureAssetData::k_assetType);
		s_textureAssetHandler = {};
		s_assetManager = nullptr;
	}
}

AssetData *TextureAssetHandler::createAsset(const AssetID &assetID, const AssetType &assetType) noexcept
{
	if (assetType != TextureAssetData::k_assetType)
	{
		Log::warn("TextureAssetHandler: Tried to call createAsset on a non-texture asset!");
		return nullptr;
	}

	return new TextureAssetData(assetID);
}

bool TextureAssetHandler::loadAssetData(AssetData *assetData, const char *path) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != TextureAssetData::k_assetType)
	{
		Log::warn("TextureAssetHandler: Tried to call loadAssetData on a non-texture asset!");
		return false;
	}

	assetData->setAssetStatus(AssetStatus::LOADING);

	// load file
	{
		if (!VirtualFileSystem::get().exists(path) || VirtualFileSystem::get().isDirectory(path))
		{
			assetData->setAssetStatus(AssetStatus::ERROR);
			Log::err("TextureAssetHandler: Texture asset data file \"%s\" could not be found!", path);
			return false;
		}

		auto fileSize = VirtualFileSystem::get().size(path);
		eastl::vector<char> fileData(fileSize);

		bool success = false;

		if (VirtualFileSystem::get().readFile(path, fileSize, fileData.data(), true))
		{
			auto handle = m_renderer->loadTexture(fileSize, fileData.data(), path);
			static_cast<TextureAssetData *>(assetData)->m_textureHandle = handle;
			success = handle != 0;
		}

		if (!success)
		{
			Log::warn("TextureAssetHandler: Failed to load texture asset!");
			assetData->setAssetStatus(AssetStatus::ERROR);
			return false;
		}
	}

	assetData->setAssetStatus(AssetStatus::READY);

	return true;
}

void TextureAssetHandler::destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != TextureAssetData::k_assetType)
	{
		Log::warn("TextureAssetHandler: Tried to call destroyAsset on a non-texture asset!");
		return;
	}

	m_renderer->destroyTexture(static_cast<TextureAssetData *>(assetData)->m_textureHandle);

	delete assetData;
}