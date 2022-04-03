#include "MaterialAssetHandler.h"
#include <assert.h>
#include "Log.h"
#include "utility/Utility.h"
#include "graphics/Renderer.h"
#include "asset/MaterialAsset.h"
#include "asset/AssetManager.h"
#include "filesystem/VirtualFileSystem.h"
#include <nlohmann/json.hpp>
#include <glm/packing.hpp>
#include <string>
#include <sstream>

static AssetManager *s_assetManager = nullptr;
static MaterialAssetHandler s_materialAssetHandler;

void MaterialAssetHandler::init(AssetManager *assetManager, Renderer *renderer) noexcept
{
	if (s_assetManager == nullptr)
	{
		s_assetManager = assetManager;
		s_materialAssetHandler.m_renderer = renderer;
		assetManager->registerAssetHandler(MaterialAssetData::k_assetType, &s_materialAssetHandler);
	}
}

void MaterialAssetHandler::shutdown() noexcept
{
	s_materialAssetHandler = {};
	s_assetManager = nullptr;
}

AssetData *MaterialAssetHandler::createAsset(const AssetID &assetID, const AssetType &assetType) noexcept
{
	if (assetType != MaterialAssetData::k_assetType)
	{
		Log::warn("MaterialAssetHandler: Tried to call createAsset on a non-material asset!");
		return nullptr;
	}

	return new MaterialAssetData(assetID);
}

bool MaterialAssetHandler::loadAssetData(AssetData *assetData, const char *path) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != MaterialAssetData::k_assetType)
	{
		Log::warn("MaterialAssetHandler: Tried to call loadAssetData on a non-material asset!");
		return false;
	}

	assetData->setAssetStatus(AssetStatus::LOADING);

	// load file
	{
		if (!VirtualFileSystem::get().exists(path) || VirtualFileSystem::get().isDirectory(path))
		{
			assetData->setAssetStatus(AssetStatus::ERROR);
			Log::err("MaterialAssetHandler: Material asset data file \"%s\" could not be found!", path);
			return false;
		}

		auto fileSize = VirtualFileSystem::get().size(path);
		eastl::vector<char> fileData(fileSize);

		bool success = false;

		if (VirtualFileSystem::get().readFile(path, fileSize, fileData.data(), true))
		{
			nlohmann::json jmat;
			std::stringstream ss(fileData.data());
			ss >> jmat;

			auto getTextureAsset = [&](const std::string &filepath) -> Asset<TextureAsset>
			{
				if (filepath.empty())
				{
					return {};
				}

				return s_assetManager->getAsset<TextureAsset>(AssetID(filepath.c_str()));
			};

			auto *materialAssetData = static_cast<MaterialAssetData *>(assetData);
			materialAssetData->m_albedoTexture = getTextureAsset(jmat["albedoTexture"].get<std::string>());
			materialAssetData->m_normalTexture = getTextureAsset(jmat["normalTexture"].get<std::string>());
			materialAssetData->m_metalnessTexture = getTextureAsset(jmat["metalnessTexture"].get<std::string>());
			materialAssetData->m_roughnessTexture = getTextureAsset(jmat["roughnessTexture"].get<std::string>());
			materialAssetData->m_occlusionTexture = getTextureAsset(jmat["occlusionTexture"].get<std::string>());
			materialAssetData->m_emissiveTexture = getTextureAsset(jmat["emissiveTexture"].get<std::string>());
			materialAssetData->m_displacementTexture = getTextureAsset(jmat["displacementTexture"].get<std::string>());

			MaterialCreateInfo material{};
			material.m_alpha = MaterialAlphaMode(jmat["alphaMode"].get<uint32_t>());
			material.m_albedoFactor = glm::packUnorm4x8(glm::vec4(jmat["albedo"][0].get<float>(), jmat["albedo"][1].get<float>(), jmat["albedo"][2].get<float>(), 1.0f));
			material.m_metallicFactor = jmat["metalness"].get<float>();
			material.m_roughnessFactor = jmat["roughness"].get<float>();
			material.m_emissiveFactor[0] = jmat["emissive"][0].get<float>();
			material.m_emissiveFactor[1] = jmat["emissive"][1].get<float>();
			material.m_emissiveFactor[2] = jmat["emissive"][2].get<float>();
			material.m_albedoTexture = materialAssetData->m_albedoTexture.isLoaded() ? materialAssetData->m_albedoTexture->getTextureHandle() : TextureHandle();
			material.m_normalTexture = materialAssetData->m_normalTexture.isLoaded() ? materialAssetData->m_normalTexture->getTextureHandle() : TextureHandle();
			material.m_metallicTexture = materialAssetData->m_metalnessTexture.isLoaded() ? materialAssetData->m_metalnessTexture->getTextureHandle() : TextureHandle();
			material.m_roughnessTexture = materialAssetData->m_roughnessTexture.isLoaded() ? materialAssetData->m_roughnessTexture->getTextureHandle() : TextureHandle();
			material.m_occlusionTexture = materialAssetData->m_occlusionTexture.isLoaded() ? materialAssetData->m_occlusionTexture->getTextureHandle() : TextureHandle();
			material.m_emissiveTexture = materialAssetData->m_emissiveTexture.isLoaded() ? materialAssetData->m_emissiveTexture->getTextureHandle() : TextureHandle();
			material.m_displacementTexture = materialAssetData->m_displacementTexture.isLoaded() ? materialAssetData->m_displacementTexture->getTextureHandle() : TextureHandle();

			MaterialHandle materialHandle = {};
			m_renderer->createMaterials(1, &material, &materialHandle);
			materialAssetData->m_materialHandle = materialHandle;
			success = materialHandle != 0;
		}

		if (!success)
		{
			Log::warn("TextureAssetHandler: Failed to load texture asset!");
			assetData->setAssetStatus(AssetStatus::ERROR);

			auto *materialAssetData = static_cast<MaterialAssetData *>(assetData);
			materialAssetData->m_albedoTexture.release();
			materialAssetData->m_normalTexture.release();
			materialAssetData->m_metalnessTexture.release();
			materialAssetData->m_roughnessTexture.release();
			materialAssetData->m_occlusionTexture.release();
			materialAssetData->m_emissiveTexture.release();
			materialAssetData->m_displacementTexture.release();

			return false;
		}
	}

	assetData->setAssetStatus(AssetStatus::READY);

	return true;
}

void MaterialAssetHandler::destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != MaterialAssetData::k_assetType)
	{
		Log::warn("MaterialAssetHandler: Tried to call destroyAsset on a non-material asset!");
		return;
	}

	delete assetData;
}
