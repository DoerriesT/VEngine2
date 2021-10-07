#include "ScriptAssetHandler.h"
#include "asset/ScriptAsset.h"
#include "asset/AssetManager.h"
#include "Log.h"
#include "filesystem/VirtualFileSystem.h"

static AssetManager *s_assetManager = nullptr;
static ScriptAssetHandler s_scriptAssetHandler;

void ScriptAssetHandler::init(AssetManager *assetManager) noexcept
{
	if (s_assetManager == nullptr)
	{
		s_assetManager = assetManager;
		assetManager->registerAssetHandler(ScriptAssetData::k_assetType, &s_scriptAssetHandler);
	}
}

void ScriptAssetHandler::shutdown() noexcept
{
	if (s_assetManager != nullptr)
	{
		s_assetManager->unregisterAssetHandler(ScriptAssetData::k_assetType);
		s_scriptAssetHandler = {};
		s_assetManager = nullptr;
	}
}

AssetData *ScriptAssetHandler::createAsset(const AssetID &assetID, const AssetType &assetType) noexcept
{
	if (assetType != ScriptAssetData::k_assetType)
	{
		Log::warn("ScriptAssetHandler: Tried to call createAsset on a non-script asset!");
		return nullptr;
	}

	return new ScriptAssetData(assetID);
}

bool ScriptAssetHandler::loadAssetData(AssetData *assetData, const char *path) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != ScriptAssetData::k_assetType)
	{
		Log::warn("ScriptAssetHandler: Tried to call loadAssetData on a non-script asset!");
		return false;
	}

	assetData->setAssetStatus(AssetStatus::LOADING);

	// load file
	{
		if (!VirtualFileSystem::get().exists(path) || VirtualFileSystem::get().isDirectory(path))
		{
			assetData->setAssetStatus(AssetStatus::ERROR);
			Log::err("ScriptAssetHandler: Script asset data file \"%s\" could not be found!", path);
			return false;
		}

		auto fileSize = VirtualFileSystem::get().size(path);
		char *fileData = new char[fileSize + 1 /*null terminator*/];

		if (VirtualFileSystem::get().readFile(path, fileSize + 1, fileData, true))
		{
			fileData[fileSize] = '\0';
			static_cast<ScriptAssetData *>(assetData)->m_scriptString = fileData;
		}
		else
		{
			delete[] fileData;
			Log::warn("ScriptAssetHandler: Failed to load script asset!");
			assetData->setAssetStatus(AssetStatus::ERROR);
			return false;
		}
	}

	assetData->setAssetStatus(AssetStatus::READY);

	return true;
}

void ScriptAssetHandler::destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != ScriptAssetData::k_assetType)
	{
		Log::warn("ScriptAssetHandler: Tried to call destroyAsset on a non-script asset!");
		return;
	}

	delete[] static_cast<ScriptAssetData *>(assetData)->m_scriptString;
	delete assetData;
}
