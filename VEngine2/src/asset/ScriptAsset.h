#pragma once
#include "Asset.h"
#include "Handles.h"

/// <summary>
/// AssetData implementation for script assets.
/// </summary>
class ScriptAsset : public AssetData
{
	friend class ScriptAssetHandler;
public:
	static constexpr AssetType k_assetType = "F8988A7D-CB75-41E1-9F88-A8B90A59D051"_uuid;

	explicit ScriptAsset(const AssetID &assetID) noexcept : AssetData(assetID, k_assetType) {}
	const char *getScriptString() const noexcept { return m_scriptString; }

private:
	const char *m_scriptString = nullptr;
};