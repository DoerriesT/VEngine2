#pragma once
#include "Asset.h"
#include "Handles.h"
#include "TextureAsset.h"

/// <summary>
/// AssetData implementation for material assets.
/// </summary>
class MaterialAsset : public AssetData
{
	friend class MaterialAssetHandler;
public:
	static constexpr AssetType k_assetType = "17024075-022D-4AEA-8FD6-BEB267A0F7DF"_uuid;

	explicit MaterialAsset(const AssetID &assetID) noexcept : AssetData(assetID, k_assetType) {}
	MaterialHandle getMaterialHandle() const noexcept { return m_materialHandle; }

private:
	MaterialHandle m_materialHandle = {};
	Asset<TextureAsset> m_albedoTexture;
	Asset<TextureAsset> m_normalTexture;
	Asset<TextureAsset> m_metalnessTexture;
	Asset<TextureAsset> m_roughnessTexture;
	Asset<TextureAsset> m_occlusionTexture;
	Asset<TextureAsset> m_emissiveTexture;
	Asset<TextureAsset> m_displacementTexture;
};