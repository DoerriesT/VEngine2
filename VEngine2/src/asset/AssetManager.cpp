#include "AssetManager.h"
#include "AssetHandler.h"
#include "Log.h"
#include "UUID.h"
#include <string>

AssetManager *AssetManager::s_instance = nullptr;

bool AssetManager::init()
{
	assert(!s_instance);
	s_instance = new AssetManager();
	return true;
}

void AssetManager::shutdown()
{
	assert(s_instance);

	Log::warn(("AssetManager still had " + std::to_string(s_instance->m_assetMap.size()) + " assets loaded when shutdown() was called!").c_str());

	// unload all remaining assets
	for (auto p : s_instance->m_assetMap)
	{
		auto assetID = p.first;
		auto assetType = p.second->getAssetType();
		auto assetData = p.second;

		auto handlerIt = s_instance->m_assetHandlerMap.find(assetType);

		// failed to find handler
		if (handlerIt == s_instance->m_assetHandlerMap.end())
		{
			Log::warn("Could not find asset handler!");
			continue;
		}

		// destroy asset data
		AssetHandler *handler = handlerIt->second;
		handler->destroyAsset(assetID, assetType, assetData);
	}

	delete s_instance;
	s_instance;
}

AssetManager *AssetManager::get()
{
	return s_instance;
}

Asset<AssetData> AssetManager::getAsset(const AssetID &assetID, const AssetType &assetType) noexcept
{
	AssetData *assetData = nullptr;

	{
		// need to hold mutex so other treads dont try to create the same asset if it couldnt be found in the map
		SpinLockHolder holder(m_assetMutex);

		// try to find asset in map
		auto assetMapIt = m_assetMap.find(assetID);

		// found asset in map
		if (assetMapIt != m_assetMap.end())
		{
			return assetMapIt->second;
		}

		// couldnt find asset -> try to load from disk
		Log::info("Loading asset.");

		const char *assetPath = nullptr;
		AssetHandler *handler = nullptr;

		// try to find asset path
		{
			SpinLockHolder holder(m_assetPathMutex);

			auto assetPathIt = m_assetIDToPath.find(assetID);

			// failed to find asset path
			if (assetPathIt == m_assetIDToPath.end())
			{
				Log::warn("Could not find asset path!");
				return nullptr;
			}

			assetPath = assetPathIt->second;
		}

		// try to find asset handler
		{
			SpinLockHolder holder(m_assetHandlerMutex);

			auto handlerIt = m_assetHandlerMap.find(assetType);

			// failed to find handler
			if (handlerIt == m_assetHandlerMap.end())
			{
				Log::warn("Could not find asset handler!");
				return nullptr;
			}

			handler = handlerIt->second;
		}

		// load asset
		AssetData *assetData = handler->createAsset(assetID, assetType);

		if (!assetData)
		{
			Log::warn("Failed to create asset!");
			return nullptr;
		}

		if (!handler->loadAssetData(assetData, assetPath))
		{
			handler->destroyAsset(assetID, assetType, assetData);
			Log::warn("Failed to load asset!");
			return nullptr;
		}

		// store in map
		m_assetMap[assetID] = assetData;
	}

	Log::info("Successfully loaded asset.");

	return assetData;
}

void AssetManager::unloadAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept
{
	Log::info("Unloading asset.");

	AssetHandler *handler = nullptr;

	// try to find asset handler
	{
		SpinLockHolder holder(m_assetHandlerMutex);

		auto handlerIt = m_assetHandlerMap.find(assetType);

		// failed to find handler
		if (handlerIt == m_assetHandlerMap.end())
		{
			Log::warn("Could not find asset handler!");
			return;
		}

		handler = handlerIt->second;
	}

	// remove from map
	bool erased = false;
	{
		SpinLockHolder holder(m_assetPathMutex);
		erased = m_assetMap.erase(assetID) >= 1;
	}

	// destroy asset data
	if (erased)
	{
		handler->destroyAsset(assetID, assetType, assetData);
	}

	Log::info("Successfully unloaded asset.");
}

void AssetManager::registerAssetHandler(const AssetType &assetType, AssetHandler *handler) noexcept
{
	SpinLockHolder holder(m_assetHandlerMutex);
	m_assetHandlerMap[assetType] = handler;
}

void AssetManager::unregisterAssetHandler(const AssetType &assetType)
{
	SpinLockHolder holder(m_assetHandlerMutex);
	m_assetHandlerMap.erase(assetType);
}
