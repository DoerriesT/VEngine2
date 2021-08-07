#pragma once
#include "Asset.h"
#include "Handles.h"

/// <summary>
/// AssetData implementation for animation clips.
/// </summary>
class AnimationClipAssetData : public AssetData
{
	friend class AnimationClipAssetHandler;
public:
	static constexpr AssetType k_assetType = "7DFD6BE8-A4EF-4BDE-AAB1-2354612ED211"_uuid;

	explicit AnimationClipAssetData(const AssetID &assetID) noexcept;
	inline AnimationClipHandle getAnimationClipHandle() const noexcept { return m_animationClipHandle; }

private:
	AnimationClipHandle m_animationClipHandle = {};
};