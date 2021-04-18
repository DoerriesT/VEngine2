#pragma once
#include "Asset.h"
#include "Handles.h"

/// <summary>
/// AssetData implementation for texture assets.
/// </summary>
class TextureAssetData : public AssetData
{
	friend class TextureAssetHandler;
public:
	static constexpr AssetType k_assetType = "B9A7ACDF-1AF9-4B2A-AE95-1546988DB0BF"_uuid;

	explicit TextureAssetData(const AssetID &assetID) noexcept;
	inline TextureHandle getTextureHandle() const noexcept { return m_textureHandle; }

private:
	TextureHandle m_textureHandle = {};
};