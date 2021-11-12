#pragma once
#include "Asset.h"
#include "Handles.h"
#include "TextureAsset.h"

/// <summary>
/// AssetData implementation for texture assets.
/// </summary>
class MaterialAssetData : public AssetData
{
	friend class MaterialAssetHandler;
public:
	static constexpr AssetType k_assetType = "17024075-022D-4AEA-8FD6-BEB267A0F7DF"_uuid;

	explicit MaterialAssetData(const AssetID &assetID) noexcept : AssetData(assetID, k_assetType) {}
	MaterialHandle getMaterialHandle() const noexcept { return m_materialHandle; }

private:
	MaterialHandle m_materialHandle = {};
	Asset<TextureAssetData> m_albedoTexture;
	Asset<TextureAssetData> m_normalTexture;
	Asset<TextureAssetData> m_metalnessTexture;
	Asset<TextureAssetData> m_roughnessTexture;
	Asset<TextureAssetData> m_occlusionTexture;
	Asset<TextureAssetData> m_emissiveTexture;
	Asset<TextureAssetData> m_displacementTexture;
};